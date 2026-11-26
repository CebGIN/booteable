#ifndef BOOTEABLE_IMAGE_BOOTIMAGEBUILDER_HPP
#define BOOTEABLE_IMAGE_BOOTIMAGEBUILDER_HPP

#include <cstddef>
#include <filesystem>
#include <string>

#include "domain/Types.hpp"

namespace booteable {

struct ImageBuildResult {
    bool ok = false;
    ImageLayout layout;
    ImageManifest manifest;
    std::uint64_t esp_bytes = 0;
    std::string error;
};

/*
    Construye una imagen GPT/FAT32 completa en `output`, con el EFI indicado.

    Orden: calcular geometría, validar configuración, crear archivo disperso con
    el tamaño exacto, escribir MBR y GPT primaria, escribir la ESP, escribir la
    GPT de respaldo, fsync, cerrar y releer con ImageVerifier. El manifiesto que
    se devuelve sale de esa relectura, no de lo que se creía haber escrito.
*/
ImageBuildResult build_boot_image(const std::filesystem::path& output,
                                  const BootImageSpec& spec,
                                  const unsigned char* efi,
                                  std::size_t efi_size);

} // namespace booteable

#endif // BOOTEABLE_IMAGE_BOOTIMAGEBUILDER_HPP
