#ifndef BOOTEABLE_CORE_SHA256_HPP
#define BOOTEABLE_CORE_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace booteable {

/*
    SHA-256 incremental. Se usa para hashear regiones de la imagen sin cargar
    el disco completo en memoria y para comparar la relectura.
*/
class Sha256 {
public:
    Sha256();

    void update(const void* data, std::size_t size);
    void update_zeros(std::uint64_t count);

    // Cierra el hash y devuelve el digest en hexadecimal (64 caracteres).
    std::string finish_hex();

    // Atajo para hashear un bloque completo.
    static std::string hex(const void* data, std::size_t size);

private:
    void transform(const std::uint8_t* block);
    void pad();

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t total_bytes_ = 0;
    bool finished_ = false;
};

std::string sha256_hex(const void* data, std::size_t size);

} // namespace booteable

#endif // BOOTEABLE_CORE_SHA256_HPP
