#include "image/ImageLayout.hpp"

#include "domain/Validation.hpp"

namespace booteable {
namespace {

constexpr std::uint64_t kGptEntriesBytes = 128ull * 128ull;
constexpr std::uint32_t kReservedSectors = 32;
constexpr std::uint32_t kFatCount = 2;
constexpr std::uint32_t kSectorsPerCluster = 1;
constexpr std::uint64_t kFat32MinimumClusters = 65525ull;
constexpr std::uint32_t kFat32MaximumFatSectors = 65535u;

void fail(LayoutResult& result, const std::string& message) {
    result.ok = false;
    result.error = message;
}

} // namespace

std::uint64_t layout_esp_bytes(const BootImageSpec& spec) {
    return spec.esp_bytes != 0 ? spec.esp_bytes : default_esp_bytes(spec.logical_sector);
}

bool compute_fat_geometry(std::uint64_t total_sectors,
                          std::uint32_t sector_size, FatGeometry& out,
                          std::string& error) {
    if (sector_size != 512 && sector_size != 4096) {
        error = "El sector lógico debe ser 512 o 4096.";
        return false;
    }

    // Se busca el menor `fat_sectors` que alcance para la cantidad de clústeres
    // resultante. Primero se sube hasta encontrar un valor válido y después se
    // baja mientras el candidato anterior siga siendo suficiente.
    const auto needed_for = [&](std::uint32_t fat) -> std::uint64_t {
        const std::uint64_t data_sectors =
            total_sectors - kReservedSectors - kFatCount * fat;
        const std::uint64_t clusters = data_sectors / kSectorsPerCluster;
        return (clusters + 2) * 4;
    };

    std::uint32_t fat_sectors = 1;
    std::uint32_t cluster_count = 0;
    bool fat_ok = false;

    for (int iteration = 0; iteration < 64; ++iteration) {
        if (total_sectors <= kReservedSectors + 2ull * fat_sectors) {
            break;
        }
        const std::uint64_t data_sectors =
            total_sectors - kReservedSectors - kFatCount * fat_sectors;
        const std::uint64_t clusters = data_sectors / kSectorsPerCluster;
        if (clusters > 0xFFFFFFFFull) {
            break;
        }
        const std::uint64_t needed_sectors =
            (needed_for(fat_sectors) + sector_size - 1) / sector_size;

        if (needed_sectors <= fat_sectors) {
            cluster_count = static_cast<std::uint32_t>(clusters);
            fat_ok = true;
            break;
        }
        fat_sectors = static_cast<std::uint32_t>(needed_sectors);
    }

    if (fat_ok) {
        while (fat_sectors > 1) {
            const std::uint32_t candidate = fat_sectors - 1;
            if (total_sectors <= kReservedSectors + 2ull * candidate) {
                break;
            }
            const std::uint64_t needed_sectors =
                (needed_for(candidate) + sector_size - 1) / sector_size;
            if (needed_sectors > candidate) {
                break;
            }
            fat_sectors = candidate;
            cluster_count = static_cast<std::uint32_t>(
                (total_sectors - kReservedSectors - kFatCount * fat_sectors) /
                kSectorsPerCluster);
        }
    }

    if (!fat_ok) {
        error = "No se pudo calcular la FAT32 de la ESP.";
        return false;
    }
    if (cluster_count < kFat32MinimumClusters) {
        error = "La ESP no alcanza los 65 525 clústeres que exige FAT32.";
        return false;
    }
    if (fat_sectors > kFat32MaximumFatSectors) {
        error = "La FAT32 supera el máximo de 65 535 sectores.";
        return false;
    }

    out.fat_sectors = fat_sectors;
    out.cluster_count = cluster_count;
    return true;
}

LayoutResult compute_layout(const BootImageSpec& spec) {
    LayoutResult result;
    ImageLayout& layout = result.layout;

    if (!is_supported_sector(spec.logical_sector)) {
        fail(result, "El sector lógico debe ser 512 o 4096.");
        return result;
    }
    if (spec.disk_bytes == 0) {
        fail(result, "El tamaño de la imagen debe ser mayor que cero.");
        return result;
    }

    const std::uint64_t sector = spec.logical_sector;
    if (spec.disk_bytes % sector != 0) {
        fail(result, "El tamaño de la imagen no es múltiplo del sector lógico.");
        return result;
    }

    const std::uint64_t esp_bytes = layout_esp_bytes(spec);
    const std::uint64_t minimum = min_esp_bytes(spec.logical_sector);
    if (esp_bytes < minimum) {
        fail(result, "La ESP debe tener al menos " +
                         std::to_string(minimum / kMib) + " MiB para este sector.");
        return result;
    }
    if (esp_bytes > kMaxEspBytes) {
        fail(result, "La ESP no puede superar los 2048 MiB.");
        return result;
    }
    if (esp_bytes % sector != 0) {
        fail(result, "La ESP debe ser múltiplo del sector lógico.");
        return result;
    }

    const std::uint64_t total_sectors = spec.disk_bytes / sector;
    const std::uint64_t entry_sectors = (kGptEntriesBytes + sector - 1) / sector;
    const std::uint64_t esp_first_lba = kMib / sector;
    std::uint64_t esp_sectors = esp_bytes / sector;

    if (total_sectors <= 2 + 2 * entry_sectors) {
        fail(result, "La imagen es demasiado pequeña para la GPT.");
        return result;
    }

    const std::uint64_t first_usable = 2 + entry_sectors;
    const std::uint64_t last_usable = total_sectors - 2 - entry_sectors;

    if (esp_first_lba < first_usable) {
        fail(result, "La ESP no puede empezar antes de la GPT primaria.");
        return result;
    }

    // Si la ESP automática no entra en un disco pequeño, se reduce al mayor
    // tamaño que quepa (siempre por encima del mínimo FAT32). Una ESP pedida
    // explícitamente que no entra es un error de configuración.
    if (esp_first_lba + esp_sectors - 1 > last_usable) {
        if (spec.esp_bytes != 0) {
            fail(result, "La ESP no entra antes de la GPT de respaldo.");
            return result;
        }
        const std::uint64_t maximum_sectors = last_usable - esp_first_lba + 1;
        if (maximum_sectors * sector < minimum) {
            fail(result, "La imagen no tiene espacio para una ESP mínima.");
            return result;
        }
        esp_sectors = maximum_sectors;
    }
    if (esp_sectors == 0) {
        fail(result, "La ESP no entra antes de la GPT de respaldo.");
        return result;
    }

    // FAT32: el mismo cálculo que usará el verificador para comparar el BPB.
    FatGeometry fat;
    std::string fat_error;
    if (!compute_fat_geometry(esp_sectors, spec.logical_sector, fat,
                              fat_error)) {
        fail(result, fat_error);
        return result;
    }

    layout.sector_size = spec.logical_sector;
    layout.total_sectors = total_sectors;
    layout.esp_first_lba = esp_first_lba;
    layout.esp_sectors = esp_sectors;
    layout.backup_header_lba = total_sectors - 1;
    layout.backup_entries_lba = total_sectors - 1 - entry_sectors;
    layout.first_usable_lba = first_usable;
    layout.last_usable_lba = last_usable;
    layout.entry_sectors = static_cast<std::uint32_t>(entry_sectors);
    layout.fat_sectors = fat.fat_sectors;
    layout.cluster_count = fat.cluster_count;
    layout.efi_path = "EFI/BOOT/BOOTX64.EFI";

    result.ok = true;
    return result;
}

} // namespace booteable
