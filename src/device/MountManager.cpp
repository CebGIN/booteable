#include "device/MountManager.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

#include <sys/mount.h>

namespace booteable {
namespace {

std::size_t depth(const std::string& path) {
    return static_cast<std::size_t>(
        std::count(path.begin(), path.end(), '/'));
}

} // namespace

MountManager::MountManager(std::filesystem::path mountinfo_path,
                           UnmountFunction unmount)
    : mountinfo_path_(std::move(mountinfo_path)),
      unmount_(std::move(unmount)) {
    if (!unmount_) {
        unmount_ = default_unmount;
    }
}

int MountManager::default_unmount(const std::string& mount_point) {
    return ::umount2(mount_point.c_str(), MNT_DETACH);
}

std::vector<MountEntry> MountManager::mounts_for(
    const DeviceInfo& device) const {
    std::vector<MountEntry> mounts =
        mounts_for_device(read_mountinfo(mountinfo_path_), device);
    std::sort(mounts.begin(), mounts.end(),
              [](const MountEntry& left, const MountEntry& right) {
                  const std::size_t left_depth = depth(left.mount_point);
                  const std::size_t right_depth = depth(right.mount_point);
                  if (left_depth != right_depth) {
                      return left_depth > right_depth;
                  }
                  return left.mount_point.size() > right.mount_point.size();
              });
    return mounts;
}

bool MountManager::has_mounts(const DeviceInfo& device) const {
    return !mounts_for(device).empty();
}

bool MountManager::unmount_all(const DeviceInfo& device,
                               std::vector<std::string>& unmounted,
                               std::string& error) {
    error.clear();
    unmounted.clear();

    const std::vector<MountEntry> before = mounts_for(device);
    if (before.empty()) {
        return true;
    }

    for (const MountEntry& entry : before) {
        unmounted.push_back(entry.mount_point);
        errno = 0;
        if (unmount_(entry.mount_point) != 0) {
            const int saved = errno;
            error = "No se pudo desmontar " + entry.mount_point + ": " +
                    (saved != 0 ? std::strerror(saved)
                                : "el desmontaje fue rechazado");
            return false;
        }
    }

    // La relectura es obligatoria: umount2 puede aceptar la llamada y dejar
    // el montaje visible por una carrera o por una capa superior.
    const std::vector<MountEntry> after = mounts_for(device);
    if (!after.empty()) {
        error = "Quedan montajes del destino después de desmontar: ";
        for (std::size_t index = 0; index < after.size(); ++index) {
            if (index != 0) {
                error += ", ";
            }
            error += after[index].mount_point;
        }
        return false;
    }
    return true;
}

} // namespace booteable
