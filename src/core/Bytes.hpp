#ifndef BOOTEABLE_CORE_BYTES_HPP
#define BOOTEABLE_CORE_BYTES_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace booteable {

// Escritura y lectura little-endian sobre buffers crudos. Las estructuras de
// GPT, MBR y FAT32 usan este orden; no se hace `reinterpret_cast` de structs
// para no depender del padding del compilador.
inline void put_le16(std::uint8_t* out, std::uint16_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFFu);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
}

inline void put_le32(std::uint8_t* out, std::uint32_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFFu);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    out[2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    out[3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

inline void put_le64(std::uint8_t* out, std::uint64_t value) {
    for (int index = 0; index < 8; ++index) {
        out[index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFu);
    }
}

inline std::uint16_t read_le16(const std::uint8_t* in) {
    return static_cast<std::uint16_t>(in[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(in[1]) << 8);
}

inline std::uint32_t read_le32(const std::uint8_t* in) {
    return static_cast<std::uint32_t>(in[0]) |
           (static_cast<std::uint32_t>(in[1]) << 8) |
           (static_cast<std::uint32_t>(in[2]) << 16) |
           (static_cast<std::uint32_t>(in[3]) << 24);
}

inline std::uint64_t read_le64(const std::uint8_t* in) {
    std::uint64_t value = 0;
    for (int index = 7; index >= 0; --index) {
        value = (value << 8) | static_cast<std::uint64_t>(in[index]);
    }
    return value;
}

// Copia una cadena ASCII a un campo de longitud fija, rellenando con el
// carácter indicado (espacios para FAT, ceros para GPT).
inline void put_fixed_ascii(std::uint8_t* out, std::size_t size,
                            const std::string& text, std::uint8_t fill = 0x20) {
    for (std::size_t index = 0; index < size; ++index) {
        out[index] = index < text.size() ? static_cast<std::uint8_t>(text[index])
                                         : fill;
    }
}

// Escribe UTF-16LE en un campo de `characters` caracteres.
inline void put_utf16le(std::uint8_t* out, std::size_t characters,
                        const std::string& text) {
    for (std::size_t index = 0; index < characters; ++index) {
        const std::uint16_t code =
            index < text.size() ? static_cast<std::uint16_t>(
                                      static_cast<unsigned char>(text[index]))
                                : 0;
        put_le16(out + index * 2, code);
    }
}

} // namespace booteable

#endif // BOOTEABLE_CORE_BYTES_HPP
