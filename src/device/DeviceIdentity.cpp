#include "device/DeviceIdentity.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace booteable {
namespace {

std::string read_first_line(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::string line;
    if (!std::getline(file, line)) {
        return {};
    }
    while (!line.empty() &&
           (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' ||
            line.back() == '\t')) {
        line.pop_back();
    }
    return line;
}

std::vector<std::string> split_spaces(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream stream(line);
    std::string field;
    while (stream >> field) {
        fields.push_back(field);
    }
    return fields;
}

bool same_node(const std::filesystem::path& left,
               const std::filesystem::path& right) {
    std::error_code error;
    const std::filesystem::path canonical_left =
        std::filesystem::weakly_canonical(left, error);
    if (error) {
        return false;
    }
    error.clear();
    const std::filesystem::path canonical_right =
        std::filesystem::weakly_canonical(right, error);
    if (error) {
        return false;
    }
    return canonical_left == canonical_right;
}

} // namespace

std::string unescape_mountinfo_field(const std::string& field) {
    std::string out;
    out.reserve(field.size());
    for (std::size_t index = 0; index < field.size(); ++index) {
        if (field[index] == '\\' && index + 3 < field.size()) {
            const char a = field[index + 1];
            const char b = field[index + 2];
            const char c = field[index + 3];
            if (a >= '0' && a <= '7' && b >= '0' && b <= '7' &&
                c >= '0' && c <= '7') {
                const int value = (a - '0') * 64 + (b - '0') * 8 + (c - '0');
                out.push_back(static_cast<char>(value));
                index += 3;
                continue;
            }
        }
        out.push_back(field[index]);
    }
    return out;
}

std::vector<MountEntry> read_mountinfo(const std::filesystem::path& path) {
    std::vector<MountEntry> entries;
    std::ifstream file(path);
    if (!file) {
        return entries;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::vector<std::string> fields = split_spaces(line);
        if (fields.size() < 7) {
            continue;
        }

        // mountinfo: id parent major:minor root mount_point options
        //            [optional...] - fstype source super_options
        std::size_t separator = 6;
        while (separator < fields.size() && fields[separator] != "-") {
            ++separator;
        }
        if (separator + 1 >= fields.size()) {
            continue;
        }

        MountEntry entry;
        entry.major_minor = fields[2];
        entry.mount_point = unescape_mountinfo_field(fields[4]);
        entry.fs_type = fields[separator + 1];
        if (separator + 2 < fields.size()) {
            entry.source = unescape_mountinfo_field(fields[separator + 2]);
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::map<std::string, std::vector<std::string>> mount_points_by_device(
    const std::vector<MountEntry>& entries) {
    std::map<std::string, std::vector<std::string>> result;
    for (const MountEntry& entry : entries) {
        result[entry.major_minor].push_back(entry.mount_point);
    }
    return result;
}

std::vector<MountEntry> mounts_for_device(
    const std::vector<MountEntry>& entries, const DeviceInfo& device) {
    std::set<std::string> identities;
    if (!device.dev_id.empty()) {
        identities.insert(device.dev_id);
    }
    for (const DevicePartition& partition : device.partitions) {
        if (!partition.dev_id.empty()) {
            identities.insert(partition.dev_id);
        }
    }

    std::vector<MountEntry> result;
    for (const MountEntry& entry : entries) {
        if (identities.count(entry.major_minor) != 0) {
            result.push_back(entry);
            continue;
        }
        // Si el fixture no trae major:minor, el punto de montaje de la
        // partición sigue siendo una prueba suficiente.
        for (const DevicePartition& partition : device.partitions) {
            if (!partition.mount_point.empty() &&
                partition.mount_point == entry.mount_point) {
                result.push_back(entry);
                break;
            }
        }
    }
    return result;
}

std::string find_usb_by_id(const std::filesystem::path& disk_by_id_root,
                           const std::filesystem::path& dev_root,
                           const std::string& device_name) {
    if (device_name.empty()) {
        return {};
    }

    const std::filesystem::path node = dev_root / device_name;
    std::error_code error;
    if (!std::filesystem::exists(disk_by_id_root, error)) {
        return {};
    }

    std::filesystem::directory_iterator iterator(disk_by_id_root, error);
    if (error) {
        return {};
    }

    for (const std::filesystem::directory_entry& entry : iterator) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("usb-", 0) != 0) {
            continue;
        }
        if (same_node(entry.path(), node)) {
            return entry.path().string();
        }

        // Fallback para fixtures sin permisos de symlink: el destino relativo
        // del enlace puede apuntar directamente al nombre del nodo.
        error.clear();
        const std::filesystem::path target =
            std::filesystem::read_symlink(entry.path(), error);
        if (!error && target.filename() == node.filename()) {
            return entry.path().string();
        }
    }
    return {};
}

std::string disk_name_for_major_minor(const std::filesystem::path& sysfs_root,
                                      const std::string& major_minor) {
    if (major_minor.empty()) {
        return {};
    }

    std::error_code error;
    const std::filesystem::path link =
        sysfs_root / "dev" / "block" / major_minor;
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(link, error);
    if (!error && std::filesystem::exists(canonical)) {
        if (std::filesystem::exists(canonical / "partition")) {
            return canonical.parent_path().filename().string();
        }
        return canonical.filename().string();
    }

    // Fallback: recorrer los discos y sus particiones comparando el archivo
    // `dev`. Funciona igual con /sys real y con un árbol de fixtures.
    const std::filesystem::path block_root = sysfs_root / "block";
    std::filesystem::directory_iterator disks(block_root, error);
    if (error) {
        return {};
    }
    for (const std::filesystem::directory_entry& disk : disks) {
        if (!disk.is_directory(error)) {
            error.clear();
            continue;
        }
        if (read_first_line(disk.path() / "dev") == major_minor) {
            return disk.path().filename().string();
        }
        std::filesystem::directory_iterator children(disk.path(), error);
        if (error) {
            error.clear();
            continue;
        }
        for (const std::filesystem::directory_entry& child : children) {
            if (read_first_line(child.path() / "dev") == major_minor) {
                return disk.path().filename().string();
            }
        }
    }
    return {};
}

std::string device_basename(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

} // namespace booteable
