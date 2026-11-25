#include "device/SysfsBlockDeviceSource.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

#include "device/DeviceIdentity.hpp"
#include "device/SystemContext.hpp"

namespace booteable {
namespace {

std::string read_text(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::string value;
    if (!std::getline(file, value)) {
        return {};
    }
    while (!value.empty() &&
           (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' ||
            value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

std::uint64_t read_u64(const std::filesystem::path& path,
                       std::uint64_t fallback = 0) {
    const std::string text = read_text(path);
    if (text.empty()) {
        return fallback;
    }
    try {
        std::size_t consumed = 0;
        const std::uint64_t value = std::stoull(text, &consumed);
        if (consumed != text.size()) {
            return fallback;
        }
        return value;
    } catch (const std::exception&) {
        return fallback;
    }
}

bool read_flag(const std::filesystem::path& path) {
    return read_text(path) == "1";
}

std::string read_all(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bool is_skipped(const std::string& name) {
    const auto starts_with = [&name](const char* prefix) {
        return name.rfind(prefix, 0) == 0;
    };
    return starts_with("loop") || starts_with("ram") || starts_with("zram") ||
           starts_with("dm-") || starts_with("md") || starts_with("sr") ||
           starts_with("fd") || starts_with("nbd");
}

DeviceTransport transport_for(const std::filesystem::path& sysfs_root,
                              const std::string& name) {
    if (name.rfind("nvme", 0) == 0) {
        return DeviceTransport::Nvme;
    }
    if (name.rfind("mmcblk", 0) == 0) {
        return DeviceTransport::Mmc;
    }
    if (name.rfind("vd", 0) == 0) {
        return DeviceTransport::Virtual;
    }

    const std::filesystem::path device_dir = sysfs_root / "block" / name;
    const std::string uevent = read_all(device_dir / "device" / "uevent");
    if (uevent.find("DRIVER=usb") != std::string::npos) {
        return DeviceTransport::Usb;
    }

    std::error_code error;
    const std::string canonical =
        std::filesystem::weakly_canonical(device_dir, error).string();
    if (!error) {
        if (canonical.find("/usb") != std::string::npos) {
            return DeviceTransport::Usb;
        }
        if (canonical.find("/virtio") != std::string::npos) {
            return DeviceTransport::Virtual;
        }
    }
    if (name.rfind("sd", 0) == 0) {
        return DeviceTransport::Sata;
    }
    return DeviceTransport::Unknown;
}

std::vector<std::string> read_holders(const std::filesystem::path& device_dir) {
    std::vector<std::string> holders;
    const std::filesystem::path holders_dir = device_dir / "holders";
    std::error_code error;
    std::filesystem::directory_iterator iterator(holders_dir, error);
    if (error) {
        return holders;
    }
    for (const std::filesystem::directory_entry& entry : iterator) {
        holders.push_back(entry.path().filename().string());
    }
    std::sort(holders.begin(), holders.end());
    return holders;
}

std::vector<DevicePartition> read_partitions(
    const std::filesystem::path& device_dir, const std::string& name,
    const std::map<std::string, std::vector<std::string>>& mounts) {
    struct Candidate {
        unsigned long number = 0;
        DevicePartition partition;
    };

    std::vector<Candidate> candidates;
    std::error_code error;
    std::filesystem::directory_iterator iterator(device_dir, error);
    if (error) {
        return {};
    }

    for (const std::filesystem::directory_entry& entry : iterator) {
        const std::string child_name = entry.path().filename().string();
        if (child_name == name || child_name.rfind(name, 0) != 0) {
            continue;
        }
        if (!std::filesystem::exists(entry.path() / "partition", error)) {
            error.clear();
            continue;
        }

        Candidate candidate;
        candidate.number = static_cast<unsigned long>(
            read_u64(entry.path() / "partition", 0));
        candidate.partition.name = child_name;
        candidate.partition.dev_id = read_text(entry.path() / "dev");
        candidate.partition.size_bytes = read_u64(entry.path() / "size") * 512ull;

        const auto found = mounts.find(candidate.partition.dev_id);
        if (found != mounts.end() && !found->second.empty()) {
            candidate.partition.mount_point = found->second.front();
        }
        candidates.push_back(std::move(candidate));
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& left, const Candidate& right) {
                  if (left.number != right.number) {
                      return left.number < right.number;
                  }
                  return left.partition.name < right.partition.name;
              });

    std::vector<DevicePartition> partitions;
    partitions.reserve(candidates.size());
    for (Candidate& candidate : candidates) {
        partitions.push_back(std::move(candidate.partition));
    }
    return partitions;
}

DeviceInfo read_device(const SysfsPaths& paths,
                       const std::map<std::string, std::vector<std::string>>& mounts,
                       const std::string& name) {
    const std::filesystem::path device_dir = paths.sysfs / "block" / name;

    DeviceInfo device;
    device.path = (paths.dev / name).string();
    device.by_id = find_usb_by_id(paths.disk_by_id, paths.dev, name);
    device.model = read_text(device_dir / "device" / "model");
    if (device.model.empty()) {
        device.model = name;
    }
    device.serial = read_text(device_dir / "device" / "serial");
    device.size_bytes = read_u64(device_dir / "size") * 512ull;
    device.logical_sector = static_cast<std::uint32_t>(
        read_u64(device_dir / "queue" / "logical_block_size", 512));
    device.removable = read_flag(device_dir / "removable");
    device.read_only = read_flag(device_dir / "ro");
    device.transport = transport_for(paths.sysfs, name);
    device.dev_id = read_text(device_dir / "dev");
    device.holders = read_holders(device_dir);
    device.partitions = read_partitions(device_dir, name, mounts);
    return device;
}

} // namespace

SysfsBlockDeviceSource::SysfsBlockDeviceSource(SysfsPaths paths)
    : paths_(std::move(paths)) {}

std::vector<DeviceInfo> SysfsBlockDeviceSource::list() {
    std::vector<std::string> names;
    std::error_code error;
    std::filesystem::directory_iterator iterator(paths_.sysfs / "block", error);
    if (error) {
        return {};
    }
    for (const std::filesystem::directory_entry& entry : iterator) {
        const std::string name = entry.path().filename().string();
        if (is_skipped(name)) {
            continue;
        }
        if (!entry.is_directory(error)) {
            error.clear();
            continue;
        }
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());

    const auto mounts = mount_points_by_device(
        read_mountinfo(paths_.mountinfo));

    std::vector<DeviceInfo> devices;
    devices.reserve(names.size());
    for (const std::string& name : names) {
        devices.push_back(read_device(paths_, mounts, name));
    }
    return devices;
}

std::optional<DeviceInfo> SysfsBlockDeviceSource::refresh(
    const std::string& path) {
    const std::string wanted = device_basename(path);
    for (DeviceInfo& device : list()) {
        if (device.path == path || device_basename(device.path) == wanted) {
            return device;
        }
    }
    return std::nullopt;
}

SystemContext SysfsBlockDeviceSource::system_context() {
    SystemContextReader reader(paths_.sysfs, paths_.mountinfo);
    return reader.read();
}

} // namespace booteable
