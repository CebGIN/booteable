#ifndef BOOTEABLE_DEVICE_IDEVICEWRITER_HPP
#define BOOTEABLE_DEVICE_IDEVICEWRITER_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "domain/Types.hpp"

namespace booteable {

// Escritor secuencial sobre el destino. El orden del flujo lo fija
// PrepRunner: desmontar, abrir, escribir, sincronizar y releer.
class IDeviceWriter {
public:
    virtual ~IDeviceWriter() = default;

    virtual bool open(const DeviceInfo& device, std::string& error) = 0;
    virtual bool write(const std::byte* data, std::size_t bytes,
                       std::string& error) = 0;
    // Posiciona el flujo en un offset absoluto. Permite escribir solo las
    // regiones del manifiesto y saltear los huecos de la imagen dispersa.
    virtual bool seek(std::uint64_t offset, std::string& error) {
        (void)offset;
        error = "El escritor no soporta posicionamiento por offset.";
        return false;
    }
    virtual bool sync(std::string& error) = 0;
    virtual bool read_back(std::byte* data, std::size_t bytes,
                           std::string& error) = 0;
    virtual void close() = 0;

    // Extensión compatible de WP3b: lectura en un offset exacto para comparar
    // las regiones del manifiesto sin recorrer la capacidad libre del medio.
    virtual bool read_at(std::uint64_t offset, std::byte* data,
                         std::size_t bytes, std::string& error) {
        (void)offset;
        (void)data;
        (void)bytes;
        error = "El escritor no soporta lectura por offset.";
        return false;
    }

    // Último errno del sistema, para traducirlo a PrepOutcome. 0 si no aplica.
    virtual int last_errno() const { return 0; }

    // Capacidad y sector reales del destino abierto. 0 si el escritor no los
    // conoce (por ejemplo, un fake en memoria).
    virtual std::uint64_t size() const { return 0; }
    virtual std::uint32_t sector_size() const { return 0; }
    virtual std::uint64_t bytes_written() const { return 0; }
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_IDEVICEWRITER_HPP
