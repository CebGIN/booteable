#include "device/SystemContext.hpp"

#include <set>
#include <utility>

#include "device/DeviceIdentity.hpp"

namespace booteable {
namespace {

std::string disk_for_mount(const std::filesystem::path& sysfs_root,
                           const std::vector<MountEntry>& entries,
                           const std::string& mount_point) {
    for (const MountEntry& entry : entries) {
        if (entry.mount_point == mount_point) {
            return disk_name_for_major_minor(sysfs_root, entry.major_minor);
        }
    }
    return {};
}

} // namespace

SystemContextReader::SystemContextReader(std::filesystem::path sysfs_root,
                                         std::filesystem::path mountinfo_path)
    : sysfs_root_(std::move(sysfs_root)),
      mountinfo_path_(std::move(mountinfo_path)) {}

SystemContext SystemContextReader::read() const {
    SystemContext context;
    const std::vector<MountEntry> entries = read_mountinfo(mountinfo_path_);

    context.root_device = disk_for_mount(sysfs_root_, entries, "/");
    context.boot_device = disk_for_mount(sysfs_root_, entries, "/boot");
    if (context.boot_device.empty()) {
        context.boot_device = context.root_device;
    }

    std::set<std::string> names;
    for (const MountEntry& entry : entries) {
        const std::string name =
            disk_name_for_major_minor(sysfs_root_, entry.major_minor);
        if (!name.empty()) {
            names.insert(name);
        }
    }
    context.mounted_device_names.assign(names.begin(), names.end());
    return context;
}

} // namespace booteable
