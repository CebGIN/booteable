#ifndef BOOTEABLE_DEVICE_SYSTEMCONTEXT_HPP
#define BOOTEABLE_DEVICE_SYSTEMCONTEXT_HPP

#include <filesystem>

#include "domain/Types.hpp"

namespace booteable {

/*
    Construye el SystemContext que usa SafetyPolicy: disco raíz, disco de
    /boot y todos los discos que tienen algún montaje. Las rutas son
    inyectables para probar la lógica con fixtures sin tocar /sys ni /proc.
*/
class SystemContextReader {
public:
    explicit SystemContextReader(
        std::filesystem::path sysfs_root = "/sys",
        std::filesystem::path mountinfo_path = "/proc/self/mountinfo");

    SystemContext read() const;

private:
    std::filesystem::path sysfs_root_;
    std::filesystem::path mountinfo_path_;
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_SYSTEMCONTEXT_HPP
