#ifndef BOOTEABLE_DEVICE_FAULTYDEVICEWRITER_HPP
#define BOOTEABLE_DEVICE_FAULTYDEVICEWRITER_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "device/IDeviceWriter.hpp"

namespace booteable {

/*
    Decorador de pruebas: envuelve un escritor real y simula fallos. Se usa
    para provocar ENODEV a mitad de escritura o un fsync fallido sin tocar
    hardware.
*/
class FaultyDeviceWriter : public IDeviceWriter {
public:
    explicit FaultyDeviceWriter(IDeviceWriter& inner);

    bool open(const DeviceInfo& device, std::string& error) override;
    bool write(const std::byte* data, std::size_t bytes,
               std::string& error) override;
    bool sync(std::string& error) override;
    bool read_back(std::byte* data, std::size_t bytes,
                   std::string& error) override;
    bool read_at(std::uint64_t offset, std::byte* data, std::size_t bytes,
                 std::string& error) override;
    void close() override;
    int last_errno() const override;

    // 0 desactiva el fallo. El error se dispara al superar `after_bytes`.
    void fail_after(std::uint64_t after_bytes, int error_number);
    void fail_sync(int error_number);
    std::uint64_t bytes_written() const override { return bytes_written_; }

private:
    IDeviceWriter& inner_;
    std::uint64_t fail_after_ = 0;
    int write_error_ = 0;
    int sync_error_ = 0;
    std::uint64_t bytes_written_ = 0;
    int last_errno_ = 0;
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_FAULTYDEVICEWRITER_HPP
