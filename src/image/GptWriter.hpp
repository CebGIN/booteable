#ifndef BOOTEABLE_IMAGE_GPTWRITER_HPP
#define BOOTEABLE_IMAGE_GPTWRITER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "domain/Types.hpp"

namespace booteable {

struct Guid {
    std::uint8_t bytes[16] = {};
};

// Convierte "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" al orden mixto que usa GPT
// (Data1/2/3 little-endian y Data4 tal cual).
bool parse_guid(const std::string& text, Guid& out);
std::string format_guid(const Guid& guid);
bool guid_is_zero(const Guid& guid);

const Guid& esp_type_guid();

// GUID fijos para que dos builds con la misma entrada sean idénticos byte a
// byte. No se generan al azar: la reproducibilidad es un criterio de WP2.
struct GptGuids {
    Guid disk;
    Guid esp;
};
GptGuids default_gpt_guids();

// MBR protector: una sola entrada tipo 0xEE que cubre el disco.
std::vector<std::uint8_t> make_protective_mbr(const ImageLayout& layout);

// GPT primaria: cabecera en LBA 1 y entradas desde LBA 2.
std::vector<std::uint8_t> make_primary_gpt(const ImageLayout& layout,
                                           const GptGuids& guids,
                                           const std::string& volume_label);

// GPT de respaldo: entradas antes de la cabecera en el último LBA.
std::vector<std::uint8_t> make_backup_gpt(const ImageLayout& layout,
                                          const GptGuids& guids,
                                          const std::string& volume_label);

struct GptHeaderInfo {
    std::uint32_t revision = 0;
    std::uint32_t header_size = 0;
    std::uint32_t header_crc32 = 0;
    std::uint64_t current_lba = 0;
    std::uint64_t backup_lba = 0;
    std::uint64_t first_usable_lba = 0;
    std::uint64_t last_usable_lba = 0;
    Guid disk_guid;
    std::uint64_t entries_lba = 0;
    std::uint32_t entry_count = 0;
    std::uint32_t entry_size = 0;
    std::uint32_t entries_crc32 = 0;
};

// Lee y valida la firma, la revisión y el CRC de una cabecera GPT.
bool parse_gpt_header(const std::uint8_t* sector, std::uint32_t sector_size,
                      GptHeaderInfo& out, std::string& error);

// Valida el CRC32 del arreglo de entradas.
bool gpt_entries_crc_ok(const std::uint8_t* entries, std::size_t bytes,
                        std::uint32_t expected);

} // namespace booteable

#endif // BOOTEABLE_IMAGE_GPTWRITER_HPP
