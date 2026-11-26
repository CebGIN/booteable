#include "core/Crc32.hpp"

namespace booteable {

std::uint32_t crc32(const void* data, std::size_t size,
                    std::uint32_t previous) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint32_t crc = previous ^ 0xFFFFFFFFu;

    for (std::size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                static_cast<std::uint32_t>(-static_cast<std::int32_t>(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

void Crc32::update(const void* data, std::size_t size) {
    state_ = crc32(data, size, state_);
}

std::uint32_t Crc32::value() const {
    return state_;
}

void Crc32::reset() {
    state_ = 0;
}

} // namespace booteable
