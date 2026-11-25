#ifndef BOOTEABLE_IMAGE_IMAGEVERIFIER_HPP
#define BOOTEABLE_IMAGE_IMAGEVERIFIER_HPP

#include <filesystem>
#include <string>

#include "domain/Types.hpp"
#include "image/IImageVerifier.hpp"

namespace booteable {

/*
    Verificador estructural de una imagen GPT/FAT32. Relee el archivo y
    comprueba MBR, GPT primaria y de respaldo, BPB, FSInfo, FAT, árbol
    EFI/BOOT y el SHA-256 del binario incrustado. Devuelve el manifiesto de
    regiones calculado sobre lo leído.
*/
class ImageVerifier : public IImageVerifier {
public:
    bool verify(const std::filesystem::path& image, ImageManifest& out,
                std::string& error) const override;

    // Verifica la estructura y además compara el manifiesto con uno esperado.
    // Se usa al releer un dispositivo después de escribir (WP3b).
    bool verify_against(const std::filesystem::path& image,
                        const ImageManifest& expected, ImageManifest& out,
                        std::string& error) const;
};

// Hash canónico de la lista de regiones. Es el valor que se compara entre la
// imagen construida y la relectura del dispositivo.
std::string canonical_manifest_hash(const ImageManifest& manifest);

// Detecta el sector lógico de una imagen GPT leyendo la firma "EFI PART" en
// 512 o en 4096. Se usa en `flash` para rechazar una imagen incompatible con
// el sector del destino.
bool detect_image_sector_size(const std::filesystem::path& image,
                              std::uint32_t& sector_size,
                              std::string& error);

} // namespace booteable

#endif // BOOTEABLE_IMAGE_IMAGEVERIFIER_HPP
