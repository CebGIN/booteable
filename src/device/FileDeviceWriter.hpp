#ifndef BOOTEABLE_DEVICE_FILEDEVICEWRITER_HPP
#define BOOTEABLE_DEVICE_FILEDEVICEWRITER_HPP

#include <string>

#include "device/PosixDeviceWriter.hpp"

namespace booteable {

/*
    Escritor sobre un archivo regular. Se usa para las pruebas de escritura y
    relectura sin una USB real y para exportar una imagen a un archivo.
    Comparte la lógica de escritura, fsync y lectura por offset con
    PosixDeviceWriter; solo cambia la apertura (puede crear o truncar).
*/
class FileDeviceWriter : public PosixDeviceWriter {
public:
    explicit FileDeviceWriter(bool create = false, bool truncate = false);

    bool open(const DeviceInfo& device, std::string& error) override;

private:
    bool create_ = false;
    bool truncate_ = false;
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_FILEDEVICEWRITER_HPP
