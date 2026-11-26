#ifndef BOOTEABLE_DEVICE_IBLOCKDEVICESOURCE_HPP
#define BOOTEABLE_DEVICE_IBLOCKDEVICESOURCE_HPP

#include <optional>
#include <string>
#include <vector>

#include "domain/Types.hpp"

namespace booteable {

// Fuente de dispositivos de bloque. La UI y el controlador no conocen sysfs:
// todo pasa por esta interfaz.
class IBlockDeviceSource {
public:
    virtual ~IBlockDeviceSource() = default;

    virtual std::vector<DeviceInfo> list() = 0;
    virtual std::optional<DeviceInfo> refresh(const std::string& path) = 0;
    virtual SystemContext system_context() = 0;
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_IBLOCKDEVICESOURCE_HPP
