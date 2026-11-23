#ifndef BOOTEABLE_DEVICE_IDEVICEWRITER_HPP
#define BOOTEABLE_DEVICE_IDEVICEWRITER_HPP

#include <cstddef>
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
    virtual bool sync(std::string& error) = 0;
    virtual bool read_back(std::byte* data, std::size_t bytes,
                           std::string& error) = 0;
    virtual void close() = 0;
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_IDEVICEWRITER_HPP
