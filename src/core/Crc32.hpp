#ifndef BOOTEABLE_CORE_CRC32_HPP
#define BOOTEABLE_CORE_CRC32_HPP

#include <cstddef>
#include <cstdint>

namespace booteable {

/*
    CRC32 de IEEE 802.3, polinomio 0xEDB88320 reflejado. Es el mismo que usa
    UEFI para la cabecera y las entradas de la GPT.
*/
std::uint32_t crc32(const void* data, std::size_t size,
                    std::uint32_t previous = 0);

class Crc32 {
public:
    void update(const void* data, std::size_t size);
    std::uint32_t value() const;
    void reset();

private:
    std::uint32_t state_ = 0;
};

} // namespace booteable

#endif // BOOTEABLE_CORE_CRC32_HPP
