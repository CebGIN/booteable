#ifndef BOOTEABLE_DEVICE_MOUNTMANAGER_HPP
#define BOOTEABLE_DEVICE_MOUNTMANAGER_HPP

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "device/DeviceIdentity.hpp"
#include "domain/Types.hpp"

namespace booteable {

/*
    Desmonta únicamente los montajes que pertenecen al disco destino o a sus
    particiones. Usa umount2(MNT_DETACH) y vuelve a leer mountinfo para
    confirmar que no queda ninguno. Si la relectura aún encuentra un montaje,
    devuelve error y el llamador no debe abrir el dispositivo.
*/
class MountManager {
public:
    using UnmountFunction = std::function<int(const std::string&)>;

    explicit MountManager(
        std::filesystem::path mountinfo_path = "/proc/self/mountinfo",
        UnmountFunction unmount = default_unmount);

    std::vector<MountEntry> mounts_for(const DeviceInfo& device) const;
    bool has_mounts(const DeviceInfo& device) const;

    // Desmonta de la ruta más profunda a la menos profunda. `unmounted`
    // devuelve los puntos que se intentaron desmontar.
    bool unmount_all(const DeviceInfo& device,
                     std::vector<std::string>& unmounted, std::string& error);

    static int default_unmount(const std::string& mount_point);

private:
    std::filesystem::path mountinfo_path_;
    UnmountFunction unmount_;
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_MOUNTMANAGER_HPP
