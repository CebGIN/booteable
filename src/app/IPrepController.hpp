#ifndef BOOTEABLE_APP_IPREPCONTROLLER_HPP
#define BOOTEABLE_APP_IPREPCONTROLLER_HPP

#include <string>
#include <vector>

#include "domain/Types.hpp"

namespace booteable {

// Contrato que consume la UI. No enumera /sys, no abre /dev y no calcula
// geometrías: solo pide datos y dispara la preparación.
class IPrepController {
public:
    virtual ~IPrepController() = default;

    virtual std::vector<DeviceInfo> devices() = 0;
    virtual SafetyReport safety(const DeviceInfo& device) = 0;
    virtual ValidationReport validate(const PrepConfig& config) = 0;
    virtual bool start(const PrepConfig& config) = 0;
    virtual void cancel() = 0;
    virtual PrepProgress progress() const = 0;
    virtual PrepResult result() const = 0;
    virtual bool running() const = 0;

    // Extensiones de WP4 con implementación por defecto: la UI necesita saber
    // si puede escribir y exportar sin conocer root, sysfs ni /dev. El fake
    // hereda los valores permisivos; PrepController los sobrescribe.
    virtual bool can_write() const { return true; }
    virtual std::string write_block_reason() const { return {}; }
    virtual bool export_image(const PrepConfig&, std::string& error) {
        error = "El controlador no permite exportar imágenes.";
        return false;
    }
};

} // namespace booteable

#endif // BOOTEABLE_APP_IPREPCONTROLLER_HPP
