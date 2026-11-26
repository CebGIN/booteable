#ifndef BOOTEABLE_IMAGE_IMAGELAYOUT_HPP
#define BOOTEABLE_IMAGE_IMAGELAYOUT_HPP

#include <string>

#include "domain/Types.hpp"

namespace booteable {

struct LayoutResult {
    bool ok = false;
    ImageLayout layout;
    std::string error;
};

/*
    Calcula la geometría de la imagen: MBR, GPT primaria, ESP alineada a 1 MiB,
    GPT de respaldo y la FAT32 que cabe dentro de la ESP.

    Reglas (Arquitectura §7.1 y §7.4):
      - sector lógico 512 o 4096; disk_bytes múltiplo del sector;
      - esp_bytes 0 usa el valor por defecto del sector (64/260 MiB);
      - ESP entre el mínimo de 48/260 MiB y 2048 MiB, múltiplo del sector;
      - ESP dentro de [first_usable_lba, last_usable_lba];
      - FAT32 con 65 525 clústeres como mínimo y menos de 65 536 sectores de FAT.
*/
LayoutResult compute_layout(const BootImageSpec& spec);

// Tamaño efectivo de la ESP que usará compute_layout.
std::uint64_t layout_esp_bytes(const BootImageSpec& spec);

struct FatGeometry {
    std::uint32_t fat_sectors = 0;
    std::uint32_t cluster_count = 0;
};

/*
    Calcula la FAT32 de una ESP con 32 sectores reservados, 2 FAT y un sector
    por clúster. Se usa al construir y al verificar: el verificador no confía
    en el BPB.
*/
bool compute_fat_geometry(std::uint64_t total_sectors,
                          std::uint32_t sector_size, FatGeometry& out,
                          std::string& error);

} // namespace booteable

#endif // BOOTEABLE_IMAGE_IMAGELAYOUT_HPP
