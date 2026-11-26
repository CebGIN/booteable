#include "image/Fat32Writer.hpp"

#include <cstring>
#include <vector>

#include "core/Bytes.hpp"

namespace booteable {
namespace {

constexpr std::uint32_t kReservedSectors = 32;
constexpr std::uint32_t kFatCount = 2;
constexpr std::uint32_t kRootCluster = 2;
constexpr std::uint32_t kEfiDirectoryCluster = 3;
constexpr std::uint32_t kBootDirectoryCluster = 4;
constexpr std::uint32_t kFirstFileCluster = 5;
constexpr std::uint32_t kEndOfChain = 0x0FFFFFFFu;
constexpr std::uint32_t kVolumeId = 0x424F4F54u; // "BOOT"

// Fecha FAT fija 2026-01-01 00:00:00: misma entrada, misma imagen.
constexpr std::uint16_t kFixedDate = 0x5C21u;
constexpr std::uint16_t kFixedTime = 0u;

std::vector<std::uint8_t> make_boot_sector(const ImageLayout& layout,
                                           const std::string& label) {
    std::vector<std::uint8_t> sector(layout.sector_size, 0);

    sector[0] = 0xEB;
    sector[1] = 0x3C;
    sector[2] = 0x90;
    put_fixed_ascii(sector.data() + 3, 8, "BOOTEABL", 0x20);
    put_le16(sector.data() + 11, static_cast<std::uint16_t>(layout.sector_size));
    sector[13] = 1; // sectores por clúster
    put_le16(sector.data() + 14, static_cast<std::uint16_t>(kReservedSectors));
    sector[16] = static_cast<std::uint8_t>(kFatCount);
    put_le16(sector.data() + 17, 0); // root_entries
    put_le16(sector.data() + 19, 0); // total_sectors_16
    sector[21] = 0xF8;               // media
    put_le16(sector.data() + 22, 0); // fat_sectors_16
    put_le16(sector.data() + 24, 63);
    put_le16(sector.data() + 26, 255);
    put_le32(sector.data() + 28,
             static_cast<std::uint32_t>(layout.esp_first_lba));
    put_le32(sector.data() + 32,
             static_cast<std::uint32_t>(layout.esp_sectors));
    put_le32(sector.data() + 36, layout.fat_sectors);
    put_le16(sector.data() + 40, 0); // ext_flags: FAT única activa
    put_le16(sector.data() + 42, 0); // versión 0.0
    put_le32(sector.data() + 44, kRootCluster);
    put_le16(sector.data() + 48, 1); // FSInfo
    put_le16(sector.data() + 50, 6); // copia del boot sector
    sector[64] = 0x80;               // drive number
    sector[65] = 0;
    sector[66] = 0x29; // firma extendida
    put_le32(sector.data() + 67, kVolumeId);
    put_fixed_ascii(sector.data() + 71, 11, label, 0x20);
    put_fixed_ascii(sector.data() + 82, 8, "FAT32", 0x20);

    sector[510] = 0x55;
    sector[511] = 0xAA;
    return sector;
}

std::vector<std::uint8_t> make_fs_info(const ImageLayout& layout,
                                       std::uint32_t free_clusters,
                                       std::uint32_t next_free) {
    std::vector<std::uint8_t> sector(layout.sector_size, 0);
    put_le32(sector.data() + 0, 0x41615252u);
    put_le32(sector.data() + 484, 0x61417272u);
    put_le32(sector.data() + 488, free_clusters);
    put_le32(sector.data() + 492, next_free);
    put_le32(sector.data() + 508, 0xAA550000u);
    return sector;
}

void set_fat_entry(std::vector<std::uint8_t>& fat, std::uint32_t cluster,
                   std::uint32_t value) {
    const std::size_t offset = static_cast<std::size_t>(cluster) * 4;
    if (offset + 4 <= fat.size()) {
        put_le32(fat.data() + offset, value & 0x0FFFFFFFu);
    }
}

void write_dir_entry(std::uint8_t* entry, const std::string& short_name,
                     std::uint8_t attributes, std::uint32_t first_cluster,
                     std::uint32_t size) {
    put_fixed_ascii(entry, 11, short_name, 0x20);
    entry[11] = attributes;
    entry[12] = 0; // NTRes
    entry[13] = 0; // tiempo de creación en décimas
    put_le16(entry + 14, kFixedTime);
    put_le16(entry + 16, kFixedDate);
    put_le16(entry + 18, kFixedDate); // último acceso
    put_le16(entry + 20,
             static_cast<std::uint16_t>((first_cluster >> 16) & 0xFFFFu));
    put_le16(entry + 22, kFixedTime);
    put_le16(entry + 24, kFixedDate);
    put_le16(entry + 26,
             static_cast<std::uint16_t>(first_cluster & 0xFFFFu));
    put_le32(entry + 28, size);
}

std::vector<std::uint8_t> make_directory(
    const ImageLayout& layout,
    const std::vector<std::vector<std::uint8_t>>& entries) {
    std::vector<std::uint8_t> cluster(layout.sector_size, 0);
    std::size_t offset = 0;
    for (const std::vector<std::uint8_t>& entry : entries) {
        if (offset + entry.size() > cluster.size()) {
            break;
        }
        std::memcpy(cluster.data() + offset, entry.data(), entry.size());
        offset += entry.size();
    }
    return cluster;
}

std::vector<std::uint8_t> make_entry(const std::string& name,
                                     std::uint8_t attributes,
                                     std::uint32_t cluster, std::uint32_t size) {
    std::vector<std::uint8_t> entry(32, 0);
    write_dir_entry(entry.data(), name, attributes, cluster, size);
    return entry;
}

} // namespace

bool write_fat32(const ImageLayout& layout, const std::string& volume_label,
                 const unsigned char* efi, std::size_t efi_size,
                 const EspSink& sink, std::string& error) {
    if (sink == nullptr) {
        error = "No hay destino para escribir la ESP.";
        return false;
    }
    if (efi == nullptr || efi_size == 0) {
        error = "El binario EFI incrustado está vacío.";
        return false;
    }
    if (volume_label.empty() || volume_label.size() > 11) {
        error = "La etiqueta debe tener entre 1 y 11 caracteres.";
        return false;
    }
    if (layout.sector_size != 512 && layout.sector_size != 4096) {
        error = "La ESP tiene un sector lógico no soportado.";
        return false;
    }
    if (layout.esp_first_lba > 0xFFFFFFFFull ||
        layout.esp_sectors > 0xFFFFFFFFull) {
        error = "La geometría de la ESP no entra en los campos FAT32.";
        return false;
    }

    const std::uint64_t cluster_size = layout.sector_size;
    const std::uint64_t file_clusters =
        (efi_size + cluster_size - 1) / cluster_size;
    if (file_clusters == 0 ||
        file_clusters > static_cast<std::uint64_t>(layout.cluster_count) - 3) {
        error = "El binario EFI no entra en la ESP.";
        return false;
    }

    const std::uint32_t last_file_cluster =
        kFirstFileCluster + static_cast<std::uint32_t>(file_clusters) - 1;
    const std::uint32_t used_clusters =
        3 + static_cast<std::uint32_t>(file_clusters);
    const std::uint32_t free_clusters = layout.cluster_count - used_clusters;
    const std::uint32_t next_free =
        last_file_cluster < layout.cluster_count + 1 ? last_file_cluster + 1
                                                     : 0xFFFFFFFFu;

    const std::vector<std::uint8_t> boot =
        make_boot_sector(layout, volume_label);
    const std::vector<std::uint8_t> fsinfo =
        make_fs_info(layout, free_clusters, next_free);

    std::vector<std::uint8_t> fat(
        static_cast<std::size_t>(layout.fat_sectors) * layout.sector_size, 0);
    set_fat_entry(fat, 0, 0x0FFFFFF8u);
    set_fat_entry(fat, 1, kEndOfChain);
    set_fat_entry(fat, kRootCluster, kEndOfChain);
    set_fat_entry(fat, kEfiDirectoryCluster, kEndOfChain);
    set_fat_entry(fat, kBootDirectoryCluster, kEndOfChain);
    for (std::uint32_t cluster = kFirstFileCluster;
         cluster <= last_file_cluster; ++cluster) {
        set_fat_entry(fat, cluster,
                      cluster == last_file_cluster ? kEndOfChain : cluster + 1);
    }

    const std::vector<std::uint8_t> root_directory = make_directory(
        layout, {make_entry(volume_label, 0x08, 0, 0),
                 make_entry("EFI", 0x10, kEfiDirectoryCluster, 0)});
    const std::vector<std::uint8_t> efi_directory = make_directory(
        layout, {make_entry(".", 0x10, kEfiDirectoryCluster, 0),
                 make_entry("..", 0x10, 0, 0),
                 make_entry("BOOT", 0x10, kBootDirectoryCluster, 0)});
    const std::vector<std::uint8_t> boot_directory = make_directory(
        layout, {make_entry(".", 0x10, kBootDirectoryCluster, 0),
                 make_entry("..", 0x10, kEfiDirectoryCluster, 0),
                 make_entry("BOOTX64 EFI", 0x20, kFirstFileCluster,
                            static_cast<std::uint32_t>(efi_size))});

    const std::uint64_t data_start =
        (static_cast<std::uint64_t>(kReservedSectors) +
         static_cast<std::uint64_t>(kFatCount) * layout.fat_sectors) *
        layout.sector_size;
    const std::uint64_t fat1_offset =
        static_cast<std::uint64_t>(kReservedSectors) * layout.sector_size;
    const std::uint64_t fat2_offset =
        fat1_offset + static_cast<std::uint64_t>(layout.fat_sectors) *
                          layout.sector_size;

    const auto write = [&sink, &error](std::uint64_t offset, const void* data,
                                       std::size_t length) {
        return sink(offset, data, length, error);
    };

    // Orden ascendente: huecos (boot code, reservado, clústeres libres) quedan
    // en cero porque el archivo se creó disperso o porque el dispositivo se
    // rellena en WP3b.
    if (!write(0, boot.data(), boot.size()) ||
        !write(static_cast<std::uint64_t>(layout.sector_size), fsinfo.data(),
               fsinfo.size()) ||
        !write(6ull * layout.sector_size, boot.data(), boot.size()) ||
        !write(7ull * layout.sector_size, fsinfo.data(), fsinfo.size()) ||
        !write(fat1_offset, fat.data(), fat.size()) ||
        !write(fat2_offset, fat.data(), fat.size()) ||
        !write(data_start, root_directory.data(), root_directory.size()) ||
        !write(data_start + cluster_size, efi_directory.data(),
               efi_directory.size()) ||
        !write(data_start + 2 * cluster_size, boot_directory.data(),
               boot_directory.size())) {
        return false;
    }

    std::vector<std::uint8_t> chunk(static_cast<std::size_t>(cluster_size), 0);
    const std::uint64_t file_start = data_start + 3 * cluster_size;
    for (std::uint64_t index = 0; index < file_clusters; ++index) {
        const std::size_t source_offset =
            static_cast<std::size_t>(index * cluster_size);
        const std::size_t remaining = efi_size - source_offset;
        const std::size_t take =
            remaining < chunk.size() ? remaining : chunk.size();

        if (take == chunk.size()) {
            if (!write(file_start + index * cluster_size,
                       efi + source_offset, take)) {
                return false;
            }
        } else {
            std::memset(chunk.data(), 0, chunk.size());
            std::memcpy(chunk.data(), efi + source_offset, take);
            if (!write(file_start + index * cluster_size, chunk.data(),
                       chunk.size())) {
                return false;
            }
        }
    }

    return true;
}

} // namespace booteable
