#ifndef BOOTEABLE_DEVICE_SYSFSBLOCKDEVICESOURCE_HPP
#define BOOTEABLE_DEVICE_SYSFSBLOCKDEVICESOURCE_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "device/IBlockDeviceSource.hpp"

namespace booteable {

// Rutas inyectables: los tests usan un árbol de fixtures en lugar de /sys.
struct SysfsPaths {
    std::filesystem::path sysfs = "/sys";
    std::filesystem::path dev = "/dev";
    std::filesystem::path mountinfo = "/proc/self/mountinfo";
    std::filesystem::path disk_by_id = "/dev/disk/by-id";
};

/*
    Enumeración real de dispositivos de bloque desde sysfs.

    Lee modelo, serie, capacidad, sector lógico, transporte, by-id, holds y
    montajes. Descarta los dispositivos virtuales que no pueden ser un destino
    (loop, ram, zram, dm, md, sr, fd). No aplica la política de seguridad: eso
    lo hace SafetyPolicy con los datos devueltos.
*/
class SysfsBlockDeviceSource : public IBlockDeviceSource {
public:
    explicit SysfsBlockDeviceSource(SysfsPaths paths = {});

    std::vector<DeviceInfo> list() override;
    std::optional<DeviceInfo> refresh(const std::string& path) override;
    SystemContext system_context() override;

    const SysfsPaths& paths() const { return paths_; }

private:
    SysfsPaths paths_;
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_SYSFSBLOCKDEVICESOURCE_HPP
