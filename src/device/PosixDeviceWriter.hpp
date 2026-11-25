#ifndef BOOTEABLE_DEVICE_POSIXDEVICEWRITER_HPP
#define BOOTEABLE_DEVICE_POSIXDEVICEWRITER_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "device/IDeviceWriter.hpp"

namespace booteable {

/*
    Escritor real sobre un dispositivo de bloque o un archivo regular.

    Abre con O_RDWR|O_CLOEXEC|O_EXCL, consulta BLKGETSIZE64/BLKSSZGET cuando es
    un bloque, escribe en orden, hace fsync + BLKFLSBUF y permite releer por
    offset para comparar el manifiesto. Los errores se conservan en
    `last_errno()` para que el runner los traduzca a PrepOutcome.
*/
class PosixDeviceWriter : public IDeviceWriter {
public:
    PosixDeviceWriter() = default;
    ~PosixDeviceWriter() override;

    PosixDeviceWriter(const PosixDeviceWriter&) = delete;
    PosixDeviceWriter& operator=(const PosixDeviceWriter&) = delete;

    bool open(const DeviceInfo& device, std::string& error) override;
    bool write(const std::byte* data, std::size_t bytes,
               std::string& error) override;
    bool sync(std::string& error) override;
    bool read_back(std::byte* data, std::size_t bytes,
                   std::string& error) override;
    bool read_at(std::uint64_t offset, std::byte* data, std::size_t bytes,
                 std::string& error) override;
    void close() override;

    int last_errno() const override { return last_errno_; }
    std::uint64_t size() const override { return size_; }
    std::uint32_t sector_size() const override { return sector_size_; }
    std::uint64_t bytes_written() const override { return bytes_written_; }
    bool is_block_device() const { return block_device_; }
    bool is_open() const { return fd_ >= 0; }

private:
    int fd_ = -1;
    std::uint64_t size_ = 0;
    std::uint32_t sector_size_ = 512;
    std::uint64_t bytes_written_ = 0;
    std::uint64_t read_offset_ = 0;
    bool block_device_ = false;
    int last_errno_ = 0;
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_POSIXDEVICEWRITER_HPP
