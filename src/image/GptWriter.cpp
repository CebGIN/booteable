#include "image/GptWriter.hpp"

#include <cstring>

#include "core/Bytes.hpp"
#include "core/Crc32.hpp"

namespace booteable {
namespace {

constexpr std::size_t kGptHeaderSize = 92;
constexpr std::size_t kGptEntrySize = 128;
constexpr std::size_t kGptEntryCount = 128;
constexpr std::size_t kGptEntriesBytes = kGptEntrySize * kGptEntryCount;

void write_guid(std::uint8_t* out, const Guid& guid) {
    std::memcpy(out, guid.bytes, sizeof(guid.bytes));
}

Guid guid_from_parts(std::uint32_t part1, std::uint16_t part2,
                     std::uint16_t part3, std::uint8_t a, std::uint8_t b,
                     std::uint8_t c, std::uint8_t d, std::uint8_t e,
                     std::uint8_t f, std::uint8_t g, std::uint8_t h) {
    Guid guid;
    put_le32(guid.bytes, part1);
    put_le16(guid.bytes + 4, part2);
    put_le16(guid.bytes + 6, part3);
    guid.bytes[8] = a;
    guid.bytes[9] = b;
    guid.bytes[10] = c;
    guid.bytes[11] = d;
    guid.bytes[12] = e;
    guid.bytes[13] = f;
    guid.bytes[14] = g;
    guid.bytes[15] = h;
    return guid;
}

std::vector<std::uint8_t> make_entry_array(const ImageLayout& layout,
                                           const GptGuids& guids,
                                           const std::string& volume_label) {
    std::vector<std::uint8_t> entries(kGptEntriesBytes, 0);

    std::uint8_t* entry = entries.data();
    write_guid(entry, esp_type_guid());
    write_guid(entry + 16, guids.esp);
    put_le64(entry + 32, layout.esp_first_lba);
    put_le64(entry + 40, layout.esp_first_lba + layout.esp_sectors - 1);
    put_le64(entry + 48, 0); // atributos
    put_utf16le(entry + 56, 36, volume_label);

    return entries;
}

std::vector<std::uint8_t> make_gpt_region(const ImageLayout& layout,
                                          const GptGuids& guids,
                                          const std::string& volume_label,
                                          bool primary) {
    const std::size_t entries_bytes =
        static_cast<std::size_t>(layout.entry_sectors) * layout.sector_size;
    std::vector<std::uint8_t> region(
        (1 + static_cast<std::size_t>(layout.entry_sectors)) *
            layout.sector_size,
        0);

    const std::vector<std::uint8_t> entries =
        make_entry_array(layout, guids, volume_label);
    if (entries.size() != entries_bytes) {
        // Con 512 y 4096 el arreglo de 16 KiB ocupa exactamente entry_sectors.
        return {};
    }

    std::uint8_t* header = nullptr;
    std::uint8_t* entries_out = nullptr;
    if (primary) {
        header = region.data();
        entries_out = region.data() + layout.sector_size;
    } else {
        entries_out = region.data();
        header = region.data() + entries_bytes;
    }

    std::memcpy(entries_out, entries.data(), entries.size());

    std::memcpy(header, "EFI PART", 8);
    put_le32(header + 8, 0x00010000u);
    put_le32(header + 12, static_cast<std::uint32_t>(kGptHeaderSize));
    // El CRC de la cabecera se calcula con el campo en cero.
    put_le32(header + 16, 0);
    put_le32(header + 20, 0);
    put_le64(header + 24, primary ? 1 : layout.backup_header_lba);
    put_le64(header + 32, primary ? layout.backup_header_lba : 1);
    put_le64(header + 40, layout.first_usable_lba);
    put_le64(header + 48, layout.last_usable_lba);
    write_guid(header + 56, guids.disk);
    put_le64(header + 72,
             primary ? 2 : layout.backup_entries_lba);
    put_le32(header + 80, static_cast<std::uint32_t>(kGptEntryCount));
    put_le32(header + 84, static_cast<std::uint32_t>(kGptEntrySize));
    put_le32(header + 88,
             crc32(entries.data(), static_cast<std::size_t>(kGptEntriesBytes)));

    const std::uint32_t header_crc =
        crc32(header, static_cast<std::size_t>(kGptHeaderSize));
    put_le32(header + 16, header_crc);

    return region;
}

} // namespace

bool parse_guid(const std::string& text, Guid& out) {
    if (text.size() != 36 || text[8] != '-' || text[13] != '-' ||
        text[18] != '-' || text[23] != '-') {
        return false;
    }

    unsigned int values[16] = {};
    int index = 0;
    for (std::size_t position = 0; position < text.size();) {
        if (text[position] == '-') {
            ++position;
            continue;
        }
        int value = 0;
        int digits = 0;
        while (position < text.size() && text[position] != '-' && digits < 2) {
            const char character = text[position];
            int digit = 0;
            if (character >= '0' && character <= '9') {
                digit = character - '0';
            } else if (character >= 'a' && character <= 'f') {
                digit = character - 'a' + 10;
            } else if (character >= 'A' && character <= 'F') {
                digit = character - 'A' + 10;
            } else {
                return false;
            }
            value = value * 16 + digit;
            ++digits;
            ++position;
        }
        if (digits == 0 || index >= 16) {
            return false;
        }
        values[index++] = static_cast<unsigned int>(value);
    }
    if (index != 16) {
        return false;
    }

    put_le32(out.bytes, static_cast<std::uint32_t>(
                            (values[0] << 24) | (values[1] << 16) |
                            (values[2] << 8) | values[3]));
    put_le16(out.bytes + 4,
             static_cast<std::uint16_t>((values[4] << 8) | values[5]));
    put_le16(out.bytes + 6,
             static_cast<std::uint16_t>((values[6] << 8) | values[7]));
    for (int part = 0; part < 8; ++part) {
        out.bytes[8 + part] = static_cast<std::uint8_t>(values[8 + part]);
    }
    return true;
}

std::string format_guid(const Guid& guid) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(36);

    auto append_byte = [&result](std::uint8_t byte) {
        result.push_back(kHex[(byte >> 4) & 0xFu]);
        result.push_back(kHex[byte & 0xFu]);
    };

    const std::uint32_t part1 = read_le32(guid.bytes);
    const std::uint16_t part2 = read_le16(guid.bytes + 4);
    const std::uint16_t part3 = read_le16(guid.bytes + 6);

    append_byte(static_cast<std::uint8_t>(part1 >> 24));
    append_byte(static_cast<std::uint8_t>(part1 >> 16));
    append_byte(static_cast<std::uint8_t>(part1 >> 8));
    append_byte(static_cast<std::uint8_t>(part1));
    result.push_back('-');
    append_byte(static_cast<std::uint8_t>(part2 >> 8));
    append_byte(static_cast<std::uint8_t>(part2));
    result.push_back('-');
    append_byte(static_cast<std::uint8_t>(part3 >> 8));
    append_byte(static_cast<std::uint8_t>(part3));
    result.push_back('-');
    for (int index = 8; index < 10; ++index) {
        append_byte(guid.bytes[index]);
    }
    result.push_back('-');
    for (int index = 10; index < 16; ++index) {
        append_byte(guid.bytes[index]);
    }
    return result;
}

bool guid_is_zero(const Guid& guid) {
    for (std::uint8_t byte : guid.bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

const Guid& esp_type_guid() {
    static const Guid guid = guid_from_parts(
        0xC12A7328u, 0xF81Fu, 0x11D2u, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E,
        0xC9, 0x3B);
    return guid;
}

GptGuids default_gpt_guids() {
    GptGuids guids;
    guids.disk = guid_from_parts(0x6F1B7C2Au, 0x9E4Du, 0x4A6Bu, 0x8F, 0x3C,
                                 0x1D, 0x2E, 0x3F, 0x4A, 0x5B, 0x6C);
    guids.esp = guid_from_parts(0x7A5C9E10u, 0x2B4Du, 0x4F68u, 0x9A, 0x3C,
                                0x5D, 0x6E, 0x7F, 0x80, 0x91, 0xA2);
    return guids;
}

std::vector<std::uint8_t> make_protective_mbr(const ImageLayout& layout) {
    std::vector<std::uint8_t> sector(layout.sector_size, 0);

    std::uint8_t* entry = sector.data() + 446;
    entry[0] = 0x00; // no arrancable
    entry[1] = 0x00; // CHS de inicio 0x000200
    entry[2] = 0x02;
    entry[3] = 0x00;
    entry[4] = 0xEE; // tipo GPT protector
    entry[5] = 0xFF; // CHS de fin 0xFFFFFF
    entry[6] = 0xFF;
    entry[7] = 0xFF;
    put_le32(entry + 8, 1);
    const std::uint64_t sectors =
        layout.total_sectors > 0 ? layout.total_sectors - 1 : 0;
    put_le32(entry + 12,
             sectors > 0xFFFFFFFFull ? 0xFFFFFFFFu
                                     : static_cast<std::uint32_t>(sectors));

    sector[510] = 0x55;
    sector[511] = 0xAA;
    return sector;
}

std::vector<std::uint8_t> make_primary_gpt(const ImageLayout& layout,
                                           const GptGuids& guids,
                                           const std::string& volume_label) {
    return make_gpt_region(layout, guids, volume_label, true);
}

std::vector<std::uint8_t> make_backup_gpt(const ImageLayout& layout,
                                          const GptGuids& guids,
                                          const std::string& volume_label) {
    return make_gpt_region(layout, guids, volume_label, false);
}

bool parse_gpt_header(const std::uint8_t* sector, std::uint32_t sector_size,
                      GptHeaderInfo& out, std::string& error) {
    if (sector == nullptr) {
        error = "No hay sector de cabecera GPT.";
        return false;
    }
    if (std::memcmp(sector, "EFI PART", 8) != 0) {
        error = "La cabecera GPT no empieza con 'EFI PART'.";
        return false;
    }

    out.revision = read_le32(sector + 8);
    out.header_size = read_le32(sector + 12);
    out.header_crc32 = read_le32(sector + 16);
    out.current_lba = read_le64(sector + 24);
    out.backup_lba = read_le64(sector + 32);
    out.first_usable_lba = read_le64(sector + 40);
    out.last_usable_lba = read_le64(sector + 48);
    std::memcpy(out.disk_guid.bytes, sector + 56, 16);
    out.entries_lba = read_le64(sector + 72);
    out.entry_count = read_le32(sector + 80);
    out.entry_size = read_le32(sector + 84);
    out.entries_crc32 = read_le32(sector + 88);

    if (out.revision != 0x00010000u) {
        error = "La revisión de la GPT no es 1.0.";
        return false;
    }
    if (out.header_size != kGptHeaderSize || out.header_size > sector_size) {
        error = "El tamaño de la cabecera GPT no es 92 bytes.";
        return false;
    }

    std::uint8_t copy[512];
    if (out.header_size > sizeof(copy)) {
        error = "La cabecera GPT es demasiado grande.";
        return false;
    }
    std::memcpy(copy, sector, out.header_size);
    put_le32(copy + 16, 0);
    if (crc32(copy, out.header_size) != out.header_crc32) {
        error = "El CRC32 de la cabecera GPT no coincide.";
        return false;
    }

    if (out.entry_size != kGptEntrySize || out.entry_count != kGptEntryCount) {
        error = "La tabla de particiones GPT no es de 128 × 128 bytes.";
        return false;
    }

    return true;
}

bool gpt_entries_crc_ok(const std::uint8_t* entries, std::size_t bytes,
                        std::uint32_t expected) {
    return crc32(entries, bytes) == expected;
}

} // namespace booteable
