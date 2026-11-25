#include "image/ImageVerifier.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include "core/Bytes.hpp"
#include "core/Sha256.hpp"
#include "image/ImageFile.hpp"
#include "image/ImageLayout.hpp"
#include "image/GptWriter.hpp"

#include "EmbeddedBootImage.hpp"

namespace booteable {
namespace {

constexpr std::uint32_t kReservedSectors = 32;
constexpr std::uint32_t kFatCount = 2;
constexpr std::uint32_t kEndOfChain = 0x0FFFFFFFu;
constexpr std::uint32_t kRootCluster = 2;
constexpr std::uint16_t kFixedDate = 0x5C21u;
constexpr std::size_t kHashChunk = 1024 * 1024;

bool all_zero(const std::uint8_t* data, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
        if (data[index] != 0) {
            return false;
        }
    }
    return true;
}

bool fail(std::string& error, const std::string& region,
          const std::string& message) {
    error = region + ": " + message;
    return false;
}

bool read_bytes(ImageFile& file, std::uint64_t offset, std::uint8_t* data,
                std::size_t length, std::string& error,
                const std::string& region) {
    std::string read_error;
    if (!file.read_at(offset, data, length, read_error)) {
        error = region + ": " + read_error;
        return false;
    }
    return true;
}

bool read_vector(ImageFile& file, std::uint64_t offset, std::size_t length,
                 std::vector<std::uint8_t>& out, std::string& error,
                 const std::string& region) {
    out.assign(length, 0);
    return read_bytes(file, offset, out.data(), out.size(), error, region);
}

std::string utf16le_label(const std::uint8_t* data, std::size_t characters) {
    std::string result;
    for (std::size_t index = 0; index < characters; ++index) {
        const std::uint16_t value = read_le16(data + index * 2);
        if (value == 0) {
            break;
        }
        if (value > 0x7E || value < 0x20) {
            return {};
        }
        result.push_back(static_cast<char>(value));
    }
    return result;
}

std::string fat_name(const std::uint8_t* entry) {
    return std::string(reinterpret_cast<const char*>(entry), 11);
}

bool name_is(const std::uint8_t* entry, const std::string& name) {
    std::uint8_t expected[11];
    put_fixed_ascii(expected, sizeof(expected), name, 0x20);
    return std::memcmp(entry, expected, sizeof(expected)) == 0;
}

struct DirectoryEntry {
    std::string name;
    std::uint8_t attributes = 0;
    std::uint32_t cluster = 0;
    std::uint32_t size = 0;
};

bool parse_directory(const std::vector<std::uint8_t>& bytes,
                     std::vector<DirectoryEntry>& entries,
                     std::string& why) {
    for (std::size_t offset = 0; offset + 32 <= bytes.size(); offset += 32) {
        const std::uint8_t* entry = bytes.data() + offset;
        if (entry[0] == 0x00) {
            for (std::size_t index = offset; index < bytes.size(); ++index) {
                if (bytes[index] != 0) {
                    why = "hay datos después del final del directorio";
                    return false;
                }
            }
            return true;
        }
        if (entry[0] == 0xE5) {
            why = "hay una entrada borrada en el directorio";
            return false;
        }

        const std::uint8_t attributes = entry[11];
        if ((attributes & 0x3Fu) == 0x0Fu) {
            why = "la imagen usa nombres largos, no soportados";
            return false;
        }
        if (entry[12] != 0 || entry[13] != 0) {
            why = "campos reservados de la entrada no son cero";
            return false;
        }
        if (read_le16(entry + 14) != 0 || read_le16(entry + 16) != kFixedDate ||
            read_le16(entry + 18) != kFixedDate ||
            read_le16(entry + 22) != 0 ||
            read_le16(entry + 24) != kFixedDate) {
            why = "la entrada no usa las fechas fijas de la imagen";
            return false;
        }

        DirectoryEntry parsed;
        parsed.name = fat_name(entry);
        parsed.attributes = attributes;
        parsed.cluster =
            (static_cast<std::uint32_t>(read_le16(entry + 20)) << 16) |
            read_le16(entry + 26);
        parsed.size = read_le32(entry + 28);
        entries.push_back(parsed);
    }
    return true;
}

std::string hash_region(ImageFile& file, std::uint64_t offset,
                        std::uint64_t length, std::string& error) {
    Sha256 hash;
    std::vector<std::uint8_t> chunk(static_cast<std::size_t>(
        length < kHashChunk ? length : kHashChunk));
    std::uint64_t remaining = length;
    std::uint64_t position = offset;

    while (remaining > 0) {
        const std::size_t take = static_cast<std::size_t>(
            remaining < chunk.size() ? remaining : chunk.size());
        if (!read_bytes(file, position, chunk.data(), take, error,
                        "Manifiesto")) {
            return {};
        }
        hash.update(chunk.data(), take);
        position += take;
        remaining -= take;
    }
    return hash.finish_hex();
}

} // namespace

std::string canonical_manifest_hash(const ImageManifest& manifest) {
    std::string canonical = "booteable-manifest-v1\n";
    canonical += std::to_string(manifest.disk_bytes) + "\n";
    for (const ImageRegion& region : manifest.regions) {
        canonical += std::to_string(region.offset) + "\t" +
                     std::to_string(region.length) + "\t" + region.sha256 +
                     "\n";
    }
    return sha256_hex(canonical.data(), canonical.size());
}

bool detect_image_sector_size(const std::filesystem::path& image,
                              std::uint32_t& sector_size,
                              std::string& error) {
    error.clear();
    sector_size = 0;

    ImageFile file;
    if (!file.open_read(image, error)) {
        return false;
    }

    std::uint8_t signature[8];
    std::string read_error;
    if (file.size() >= 512 + sizeof(signature) &&
        file.read_at(512, signature, sizeof(signature), read_error) &&
        std::memcmp(signature, "EFI PART", 8) == 0) {
        sector_size = 512;
        return true;
    }
    read_error.clear();
    if (file.size() >= 4096 + sizeof(signature) &&
        file.read_at(4096, signature, sizeof(signature), read_error) &&
        std::memcmp(signature, "EFI PART", 8) == 0) {
        sector_size = 4096;
        return true;
    }

    error = "No se encontró una cabecera GPT primaria en 512 ni en 4096.";
    return false;
}

bool ImageVerifier::verify(const std::filesystem::path& image,
                           ImageManifest& out, std::string& error) const {
    error.clear();

    ImageFile file;
    if (!file.open_read(image, error)) {
        return false;
    }

    const std::uint64_t file_bytes = file.size();
    if (file_bytes < 1024) {
        return fail(error, "Imagen", "el archivo es demasiado pequeño.");
    }

    // --- MBR y tamaño de sector ------------------------------------------------
    std::uint8_t first_bytes[512];
    if (!read_bytes(file, 0, first_bytes, sizeof(first_bytes), error, "MBR")) {
        return false;
    }
    if (first_bytes[510] != 0x55 || first_bytes[511] != 0xAA) {
        return fail(error, "MBR", "falta la firma 0x55AA.");
    }
    if (!all_zero(first_bytes, 446)) {
        return fail(error, "MBR", "el código de arranque debe estar en cero.");
    }

    const std::uint8_t* mbr_entry = first_bytes + 446;
    if (mbr_entry[0] != 0x00 || mbr_entry[4] != 0xEE) {
        return fail(error, "MBR", "no hay una entrada protectora tipo 0xEE.");
    }
    if (mbr_entry[1] != 0x00 || mbr_entry[2] != 0x02 ||
        mbr_entry[3] != 0x00 || mbr_entry[5] != 0xFF ||
        mbr_entry[6] != 0xFF || mbr_entry[7] != 0xFF) {
        return fail(error, "MBR", "los CHS de la entrada protectora no son los esperados.");
    }
    if (read_le32(mbr_entry + 8) != 1) {
        return fail(error, "MBR", "la partición protectora no empieza en LBA 1.");
    }
    if (!all_zero(first_bytes + 462, 510 - 462)) {
        return fail(error, "MBR", "hay entradas de partición extra.");
    }

    std::uint32_t sector_size = 0;
    std::uint8_t signature[8];
    if (!read_bytes(file, 512, signature, sizeof(signature), error, "GPT")) {
        return false;
    }
    if (std::memcmp(signature, "EFI PART", 8) == 0) {
        sector_size = 512;
    } else if (file_bytes >= 4096 + 8) {
        if (!read_bytes(file, 4096, signature, sizeof(signature), error, "GPT")) {
            return false;
        }
        if (std::memcmp(signature, "EFI PART", 8) == 0) {
            sector_size = 4096;
        }
    }
    if (sector_size == 0) {
        return fail(error, "GPT", "no se encontró la cabecera primaria.");
    }
    if (file_bytes % sector_size != 0) {
        return fail(error, "Imagen", "el tamaño no es múltiplo del sector lógico.");
    }

    const std::uint64_t total_sectors = file_bytes / sector_size;
    if (total_sectors <= 2 + 2ull * (128ull * 128ull / sector_size)) {
        return fail(error, "Imagen", "es demasiado pequeña para la GPT.");
    }

    std::vector<std::uint8_t> mbr;
    if (!read_vector(file, 0, sector_size, mbr, error, "MBR")) {
        return false;
    }
    if (!all_zero(mbr.data() + 462, 510 - 462) ||
        !all_zero(mbr.data() + 512, sector_size - 512)) {
        return fail(error, "MBR",
                    "el sector tiene datos fuera de la entrada protectora.");
    }
    const std::uint64_t mbr_sectors = std::min<std::uint64_t>(
        total_sectors - 1, 0xFFFFFFFFull);
    if (read_le32(mbr.data() + 446 + 12) != mbr_sectors) {
        return fail(error, "MBR", "la cantidad de sectores protectores no coincide.");
    }

    // --- GPT primaria ----------------------------------------------------------
    std::vector<std::uint8_t> header_sector;
    if (!read_vector(file, sector_size, sector_size, header_sector, error,
                     "GPT primaria")) {
        return false;
    }

    GptHeaderInfo primary;
    if (!parse_gpt_header(header_sector.data(), sector_size, primary, error)) {
        return false;
    }
    if (!all_zero(header_sector.data() + primary.header_size,
                  sector_size - primary.header_size)) {
        return fail(error, "GPT primaria",
                    "la cabecera tiene bytes distintos de cero después de 92.");
    }

    const std::uint64_t entry_sectors = 128ull * 128ull / sector_size;
    if (entry_sectors * sector_size != 128ull * 128ull) {
        return fail(error, "GPT", "el arreglo de entradas no entra en sectores completos.");
    }
    const std::uint64_t expected_first_usable = 2 + entry_sectors;
    const std::uint64_t expected_last_usable = total_sectors - 2 - entry_sectors;

    if (primary.current_lba != 1 || primary.backup_lba != total_sectors - 1 ||
        primary.first_usable_lba != expected_first_usable ||
        primary.last_usable_lba != expected_last_usable ||
        primary.entries_lba != 2) {
        return fail(error, "GPT primaria", "los LBA de la cabecera no son los esperados.");
    }
    if (guid_is_zero(primary.disk_guid)) {
        return fail(error, "GPT primaria", "el GUID del disco está en cero.");
    }

    std::vector<std::uint8_t> entries;
    if (!read_vector(file, primary.entries_lba * sector_size,
                     static_cast<std::size_t>(primary.entry_count) *
                         primary.entry_size,
                     entries, error, "GPT primaria")) {
        return false;
    }
    if (!gpt_entries_crc_ok(entries.data(), entries.size(),
                            primary.entries_crc32)) {
        return fail(error, "GPT primaria", "el CRC32 de las entradas no coincide.");
    }

    Guid esp_type;
    std::memcpy(esp_type.bytes, entries.data(), 16);
    Guid esp_unique;
    std::memcpy(esp_unique.bytes, entries.data() + 16, 16);
    const std::uint64_t esp_first_i = read_le64(entries.data() + 32);
    const std::uint64_t esp_last_i = read_le64(entries.data() + 40);
    const std::uint64_t attributes = read_le64(entries.data() + 48);

    if (std::memcmp(esp_type.bytes, esp_type_guid().bytes, 16) != 0) {
        return fail(error, "GPT primaria", "la entrada 0 no es una ESP.");
    }
    if (guid_is_zero(esp_unique)) {
        return fail(error, "GPT primaria", "la ESP no tiene GUID único.");
    }
    if (attributes != 0) {
        return fail(error, "GPT primaria", "la ESP tiene atributos inesperados.");
    }
    if (!all_zero(entries.data() + 128, entries.size() - 128)) {
        return fail(error, "GPT primaria", "hay entradas de partición extra.");
    }

    const std::uint64_t expected_esp_first = kMib / sector_size;
    if (esp_first_i != expected_esp_first) {
        return fail(error, "GPT primaria", "la ESP no está alineada a 1 MiB.");
    }
    if (esp_last_i < esp_first_i ||
        esp_last_i > primary.last_usable_lba) {
        return fail(error, "GPT primaria", "los límites de la ESP se salen de la GPT.");
    }
    const std::uint64_t esp_sectors = esp_last_i - esp_first_i + 1;
    const std::uint64_t esp_bytes = esp_sectors * sector_size;
    const std::uint64_t minimum_esp =
        sector_size == 4096 ? kMinEspBytes4Kn : kMinEspBytes512;
    if (esp_bytes < minimum_esp || esp_bytes > kMaxEspBytes) {
        return fail(error, "GPT primaria", "el tamaño de la ESP no es válido.");
    }

    // --- GPT de respaldo -------------------------------------------------------
    const std::uint64_t backup_entries_lba = total_sectors - 1 - entry_sectors;
    std::vector<std::uint8_t> backup_entries;
    if (!read_vector(file, backup_entries_lba * sector_size, entries.size(),
                     backup_entries, error, "GPT de respaldo")) {
        return false;
    }
    if (backup_entries != entries) {
        return fail(error, "GPT de respaldo",
                    "las entradas no coinciden con las primarias.");
    }

    std::vector<std::uint8_t> backup_header_sector;
    if (!read_vector(file, (total_sectors - 1) * sector_size, sector_size,
                     backup_header_sector, error, "GPT de respaldo")) {
        return false;
    }
    GptHeaderInfo backup;
    if (!parse_gpt_header(backup_header_sector.data(), sector_size, backup,
                          error)) {
        return false;
    }
    if (!all_zero(backup_header_sector.data() + backup.header_size,
                  sector_size - backup.header_size)) {
        return fail(error, "GPT de respaldo",
                    "la cabecera tiene bytes distintos de cero después de 92.");
    }
    if (backup.current_lba != total_sectors - 1 || backup.backup_lba != 1 ||
        backup.entries_lba != backup_entries_lba ||
        backup.first_usable_lba != expected_first_usable ||
        backup.last_usable_lba != expected_last_usable ||
        backup.entry_count != primary.entry_count ||
        backup.entry_size != primary.entry_size ||
        backup.entries_crc32 != primary.entries_crc32 ||
        std::memcmp(backup.disk_guid.bytes, primary.disk_guid.bytes, 16) != 0) {
        return fail(error, "GPT de respaldo", "no es coherente con la primaria.");
    }

    // --- BPB de la ESP ---------------------------------------------------------
    const std::uint64_t esp_offset = esp_first_i * sector_size;
    std::vector<std::uint8_t> boot_sector;
    if (!read_vector(file, esp_offset, sector_size, boot_sector, error, "ESP")) {
        return false;
    }

    if (boot_sector[0] != 0xEB || boot_sector[1] != 0x3C ||
        boot_sector[2] != 0x90) {
        return fail(error, "ESP", "el boot sector no tiene el salto FAT32 esperado.");
    }
    if (read_le16(boot_sector.data() + 11) != sector_size ||
        boot_sector[13] != 1 || read_le16(boot_sector.data() + 14) != 32 ||
        boot_sector[16] != 2 || read_le16(boot_sector.data() + 17) != 0 ||
        read_le16(boot_sector.data() + 19) != 0 || boot_sector[21] != 0xF8 ||
        read_le16(boot_sector.data() + 22) != 0 ||
        read_le32(boot_sector.data() + 28) != esp_first_i ||
        read_le32(boot_sector.data() + 32) != esp_sectors ||
        read_le16(boot_sector.data() + 40) != 0 ||
        read_le16(boot_sector.data() + 42) != 0 ||
        read_le32(boot_sector.data() + 44) != kRootCluster ||
        read_le16(boot_sector.data() + 48) != 1 ||
        read_le16(boot_sector.data() + 50) != 6 || boot_sector[64] != 0x80 ||
        boot_sector[65] != 0 || boot_sector[66] != 0x29 ||
        read_le32(boot_sector.data() + 67) != 0x424F4F54u) {
        return fail(error, "ESP", "los parámetros del BPB no son los esperados.");
    }
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        return fail(error, "ESP", "el boot sector no tiene la firma 0x55AA.");
    }
    if (!all_zero(boot_sector.data() + 90, 510 - 90) ||
        !all_zero(boot_sector.data() + 512, sector_size - 512)) {
        return fail(error, "ESP", "el boot sector tiene bytes fuera de lugar.");
    }
    if (std::memcmp(boot_sector.data() + 82, "FAT32   ", 8) != 0) {
        return fail(error, "ESP", "el tipo de sistema de archivos no es FAT32.");
    }

    FatGeometry fat_geometry;
    std::string fat_error;
    if (!compute_fat_geometry(esp_sectors, sector_size, fat_geometry,
                              fat_error)) {
        return fail(error, "ESP", fat_error);
    }
    if (read_le32(boot_sector.data() + 36) != fat_geometry.fat_sectors) {
        return fail(error, "ESP", "el tamaño de la FAT no coincide con la geometría.");
    }

    std::string gpt_label = utf16le_label(entries.data() + 56, 36);
    std::string bpb_label(
        reinterpret_cast<const char*>(boot_sector.data() + 71), 11);
    while (!gpt_label.empty() && gpt_label.back() == ' ') {
        gpt_label.pop_back();
    }
    while (!bpb_label.empty() && bpb_label.back() == ' ') {
        bpb_label.pop_back();
    }
    if (gpt_label.empty() || gpt_label != bpb_label) {
        return fail(error, "ESP", "la etiqueta de la ESP no coincide con la GPT.");
    }

    // --- FSInfo ---------------------------------------------------------------
    std::vector<std::uint8_t> fsinfo;
    std::vector<std::uint8_t> fsinfo_copy;
    if (!read_vector(file, esp_offset + sector_size, sector_size, fsinfo, error,
                     "ESP") ||
        !read_vector(file, esp_offset + 7ull * sector_size, sector_size,
                     fsinfo_copy, error, "ESP")) {
        return false;
    }
    if (fsinfo != fsinfo_copy) {
        return fail(error, "ESP", "la copia del FSInfo no coincide.");
    }
    if (read_le32(fsinfo.data()) != 0x41615252u ||
        read_le32(fsinfo.data() + 484) != 0x61417272u ||
        read_le32(fsinfo.data() + 508) != 0xAA550000u) {
        return fail(error, "ESP", "las firmas del FSInfo no son válidas.");
    }
    const std::uint32_t free_clusters = read_le32(fsinfo.data() + 488);
    const std::uint32_t next_free = read_le32(fsinfo.data() + 492);
    if (free_clusters > fat_geometry.cluster_count ||
        (next_free != 0xFFFFFFFFu &&
         (next_free < 2 || next_free > fat_geometry.cluster_count + 1))) {
        return fail(error, "ESP", "el FSInfo tiene valores fuera de rango.");
    }
    if (!all_zero(fsinfo.data() + 4, 480) ||
        !all_zero(fsinfo.data() + 496, 12) ||
        !all_zero(fsinfo.data() + 512, sector_size - 512)) {
        return fail(error, "ESP", "el FSInfo tiene bytes fuera de lugar.");
    }

    // --- FAT doble ------------------------------------------------------------
    const std::uint64_t fat_bytes =
        static_cast<std::uint64_t>(fat_geometry.fat_sectors) * sector_size;
    std::vector<std::uint8_t> fat1;
    std::vector<std::uint8_t> fat2;
    if (!read_vector(file, esp_offset + kReservedSectors * sector_size,
                     static_cast<std::size_t>(fat_bytes), fat1, error, "FAT") ||
        !read_vector(file,
                     esp_offset +
                         (kReservedSectors + fat_geometry.fat_sectors) *
                             sector_size,
                     static_cast<std::size_t>(fat_bytes), fat2, error, "FAT")) {
        return false;
    }
    if (fat1 != fat2) {
        return fail(error, "FAT", "las dos copias de la FAT no son idénticas.");
    }

    const std::size_t fat_entries = fat1.size() / 4;
    for (std::size_t index = 0; index < fat_entries; ++index) {
        const std::uint32_t raw = read_le32(fat1.data() + index * 4);
        if ((raw & 0xF0000000u) != 0) {
            return fail(error, "FAT", "una entrada usa los bits reservados.");
        }
    }
    if (read_le32(fat1.data()) != 0x0FFFFFF8u ||
        read_le32(fat1.data() + 4) != kEndOfChain) {
        return fail(error, "FAT", "las dos primeras entradas no son las esperadas.");
    }

    const auto fat_value = [&fat1](std::uint32_t cluster) {
        return read_le32(fat1.data() + static_cast<std::size_t>(cluster) * 4) &
               0x0FFFFFFFu;
    };

    const std::uint64_t data_start_sector =
        kReservedSectors + static_cast<std::uint64_t>(kFatCount) *
                                fat_geometry.fat_sectors;
    const std::uint64_t cluster_bytes = sector_size;
    const std::uint32_t maximum_cluster = fat_geometry.cluster_count + 1;

    const auto cluster_offset = [&](std::uint32_t cluster) {
        return esp_offset +
               (data_start_sector +
                (static_cast<std::uint64_t>(cluster) - 2)) *
                   cluster_bytes;
    };

    const auto walk_chain = [&](std::uint32_t start,
                                std::vector<std::uint32_t>& chain,
                                std::string& why) {
        std::vector<std::uint8_t> seen(maximum_cluster + 1, 0);
        std::uint32_t current = start;
        while (true) {
            if (current < 2 || current > maximum_cluster) {
                why = "la cadena apunta a un clúster fuera de rango";
                return false;
            }
            if (seen[current] != 0) {
                why = "la cadena de clústeres tiene un ciclo";
                return false;
            }
            seen[current] = 1;
            chain.push_back(current);

            const std::uint32_t next = fat_value(current);
            if (next == kEndOfChain) {
                return true;
            }
            if (next < 2 || next > maximum_cluster) {
                why = "la FAT tiene un enlace inválido";
                return false;
            }
            current = next;
        }
    };

    std::vector<std::uint8_t> used(maximum_cluster + 1, 0);

    const auto read_directory = [&](std::uint32_t start,
                                    std::vector<std::uint8_t>& bytes,
                                    std::vector<DirectoryEntry>& parsed,
                                    const std::string& region,
                                    std::string& why) {
        std::vector<std::uint32_t> chain;
        if (!walk_chain(start, chain, why)) {
            error = region + ": " + why;
            return false;
        }
        if (chain.size() != 1) {
            error = region + ": el directorio debe ocupar un solo clúster.";
            return false;
        }
        bytes.assign(cluster_bytes, 0);
        for (std::size_t index = 0; index < chain.size(); ++index) {
            used[chain[index]] = 1;
            if (!read_bytes(file, cluster_offset(chain[index]),
                            bytes.data() + index * cluster_bytes,
                            static_cast<std::size_t>(cluster_bytes), error,
                            region)) {
                return false;
            }
        }
        if (!parse_directory(bytes, parsed, why)) {
            error = region + ": " + why;
            return false;
        }
        return true;
    };

    std::vector<std::uint8_t> root_bytes;
    std::vector<DirectoryEntry> root_entries;
    std::string why;
    if (!read_directory(kRootCluster, root_bytes, root_entries,
                        "Directorio raíz", why)) {
        return false;
    }
    if (root_entries.size() != 2) {
        return fail(error, "Directorio raíz", "no tiene exactamente la etiqueta y EFI.");
    }
    if (!name_is(root_bytes.data(), gpt_label) ||
        (root_entries[0].attributes & 0x3Fu) != 0x08 ||
        root_entries[0].cluster != 0 || root_entries[0].size != 0) {
        return fail(error, "Directorio raíz", "la entrada de etiqueta no es válida.");
    }
    if (root_entries[1].name != "EFI        " ||
        (root_entries[1].attributes & 0x3Fu) != 0x10 ||
        root_entries[1].cluster != 3 || root_entries[1].size != 0) {
        return fail(error, "Directorio raíz", "la entrada EFI no es válida.");
    }

    std::vector<std::uint8_t> efi_bytes;
    std::vector<DirectoryEntry> efi_entries;
    if (!read_directory(3, efi_bytes, efi_entries, "Directorio EFI", why)) {
        return false;
    }
    if (efi_entries.size() != 3 || efi_entries[0].name != ".          " ||
        efi_entries[1].name != "..         " ||
        efi_entries[2].name != "BOOT       " ||
        (efi_entries[0].attributes & 0x3Fu) != 0x10 ||
        (efi_entries[1].attributes & 0x3Fu) != 0x10 ||
        (efi_entries[2].attributes & 0x3Fu) != 0x10 ||
        efi_entries[0].cluster != 3 ||
        efi_entries[1].cluster != 0 ||
        efi_entries[2].cluster != 4 ||
        efi_entries[0].size != 0 || efi_entries[1].size != 0 ||
        efi_entries[2].size != 0) {
        return fail(error, "Directorio EFI", "el árbol no es el esperado.");
    }

    std::vector<std::uint8_t> boot_bytes;
    std::vector<DirectoryEntry> boot_entries;
    if (!read_directory(4, boot_bytes, boot_entries, "Directorio BOOT", why)) {
        return false;
    }
    if (boot_entries.size() != 3 ||
        boot_entries[0].name != ".          " ||
        boot_entries[1].name != "..         " ||
        boot_entries[2].name != "BOOTX64 EFI" ||
        (boot_entries[0].attributes & 0x3Fu) != 0x10 ||
        (boot_entries[1].attributes & 0x3Fu) != 0x10 ||
        (boot_entries[2].attributes & 0x3Fu) != 0x20 ||
        boot_entries[0].cluster != 4 ||
        boot_entries[1].cluster != 3 ||
        boot_entries[0].size != 0 || boot_entries[1].size != 0 ||
        boot_entries[2].size != kBootEfiSize ||
        boot_entries[2].cluster < 5) {
        return fail(error, "Directorio BOOT", "la entrada BOOTX64.EFI no es válida.");
    }

    std::vector<std::uint32_t> file_chain;
    if (!walk_chain(boot_entries[2].cluster, file_chain, why)) {
        return fail(error, "BOOTX64.EFI", why);
    }
    for (std::uint32_t cluster : file_chain) {
        used[cluster] = 1;
    }
    const std::uint64_t expected_clusters =
        (kBootEfiSize + cluster_bytes - 1) / cluster_bytes;
    if (file_chain.size() != expected_clusters) {
        return fail(error, "BOOTX64.EFI", "la cadena no cubre el tamaño del archivo.");
    }

    Sha256 file_hash;
    std::vector<std::uint8_t> cluster_buffer(
        static_cast<std::size_t>(cluster_bytes), 0);
    std::uint64_t remaining = kBootEfiSize;
    for (std::uint32_t cluster : file_chain) {
        if (!read_bytes(file, cluster_offset(cluster), cluster_buffer.data(),
                        cluster_buffer.size(), error, "BOOTX64.EFI")) {
            return false;
        }
        const std::size_t take = static_cast<std::size_t>(
            remaining < cluster_buffer.size() ? remaining
                                             : cluster_buffer.size());
        file_hash.update(cluster_buffer.data(), take);
        if (take < cluster_buffer.size() &&
            !all_zero(cluster_buffer.data() + take,
                      cluster_buffer.size() - take)) {
            return fail(error, "BOOTX64.EFI",
                        "el relleno del último clúster no está en cero.");
        }
        remaining -= take;
    }
    if (remaining != 0 || file_hash.finish_hex() != kBootEfiSha256) {
        return fail(error, "BOOTX64.EFI",
                    "el contenido no coincide con el EFI incrustado.");
    }

    // Toda entrada FAT fuera de las cadenas usadas debe estar libre. Así un
    // cambio en cualquier byte de la FAT también hace fallar la verificación.
    for (std::uint32_t cluster = 2; cluster <= fat_geometry.cluster_count + 1;
         ++cluster) {
        if (used[cluster] != 0) {
            continue;
        }
        if (fat_value(cluster) != 0) {
            return fail(error, "FAT", "hay un clúster libre marcado como usado.");
        }
    }
    for (std::size_t index = fat_geometry.cluster_count + 2;
         index < fat_entries; ++index) {
        if (read_le32(fat1.data() + index * 4) != 0) {
            return fail(error, "FAT", "hay entradas fuera del rango de clústeres.");
        }
    }

    // --- Manifiesto -----------------------------------------------------------
    const std::uint64_t primary_gpt_bytes =
        (1 + entry_sectors) * sector_size;
    const std::uint64_t backup_gpt_bytes = primary_gpt_bytes;
    if (esp_offset < primary_gpt_bytes ||
        esp_offset + esp_bytes >
            backup_entries_lba * sector_size) {
        return fail(error, "Manifiesto", "las regiones se solapan.");
    }

    out = ImageManifest{};
    out.disk_bytes = file_bytes;
    const struct {
        std::uint64_t offset;
        std::uint64_t length;
    } regions[] = {
        {0, sector_size},
        {sector_size, primary_gpt_bytes},
        {esp_offset, esp_bytes},
        {backup_entries_lba * sector_size, backup_gpt_bytes},
    };

    for (const auto& region : regions) {
        ImageRegion item;
        item.offset = region.offset;
        item.length = region.length;
        item.sha256 = hash_region(file, region.offset, region.length, error);
        if (item.sha256.empty()) {
            return false;
        }
        out.regions.push_back(item);
    }
    out.manifest_sha256 = canonical_manifest_hash(out);

    return true;
}

bool ImageVerifier::verify_against(const std::filesystem::path& image,
                                   const ImageManifest& expected,
                                   ImageManifest& out,
                                   std::string& error) const {
    if (!verify(image, out, error)) {
        return false;
    }
    if (out.disk_bytes != expected.disk_bytes ||
        out.regions.size() != expected.regions.size()) {
        return fail(error, "Manifiesto", "el tamaño o las regiones no coinciden.");
    }
    for (std::size_t index = 0; index < out.regions.size(); ++index) {
        const ImageRegion& actual = out.regions[index];
        const ImageRegion& wanted = expected.regions[index];
        if (actual.offset != wanted.offset || actual.length != wanted.length ||
            actual.sha256 != wanted.sha256) {
            return fail(error, "Manifiesto",
                        "la región " + std::to_string(index) +
                            " no coincide con la imagen construida.");
        }
    }
    if (out.manifest_sha256 != expected.manifest_sha256) {
        return fail(error, "Manifiesto", "el hash canónico no coincide.");
    }
    return true;
}

} // namespace booteable
