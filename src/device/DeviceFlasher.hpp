#ifndef BOOTEABLE_DEVICE_DEVICEFLASHER_HPP
#define BOOTEABLE_DEVICE_DEVICEFLASHER_HPP

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "device/IDeviceWriter.hpp"
#include "device/MountManager.hpp"
#include "domain/Types.hpp"

namespace booteable {

struct FlashCallbacks {
    // Relee la identidad del dispositivo antes de abrir y después de cerrar.
    // Si devuelve nullopt, el dispositivo desapareció.
    std::function<std::optional<DeviceInfo>(const std::string&)> refresh;
    const std::atomic_bool* cancel = nullptr;
    std::function<void(const PrepProgress&)> progress;
};

/*
    Flujo de escritura de WP3b:
    validar identidad → desmontar → abrir → escribir → sincronizar → verificar.

    No construye la imagen: recibe el manifiesto ya verificado por
    BootImageBuilder/ImageVerifier. Devuelve PrepResult con la etapa exacta y
    nunca declara Verified si la relectura no coincide.
*/
PrepResult flash_image_to_device(const DeviceInfo& device,
                                 const std::filesystem::path& image,
                                 const ImageManifest& expected,
                                 IDeviceWriter& writer, MountManager& mounts,
                                 const FlashCallbacks& callbacks = {});

} // namespace booteable

#endif // BOOTEABLE_DEVICE_DEVICEFLASHER_HPP
