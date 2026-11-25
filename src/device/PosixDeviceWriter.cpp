#include "device/PosixDeviceWriter.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/fs.h>

namespace booteable {
namespace {

std::string error_text(const std::string& action, int code) {
    return action + ": " + std::strerror(code);
}

} // namespace

PosixDeviceWriter::~PosixDeviceWriter() {
    close();
}

bool PosixDeviceWriter::open(const DeviceInfo& device, std::string& error) {
    error.clear();
    last_errno_ = 0;
    if (fd_ >= 0) {
        error = "El dispositivo ya está abierto.";
        last_errno_ = EBUSY;
        return false;
    }
    if (device.path.empty()) {
        error = "El dispositivo no tiene ruta.";
        last_errno_ = EINVAL;
        return false;
    }

    const int descriptor =
        ::open(device.path.c_str(), O_RDWR | O_CLOEXEC | O_EXCL);
    if (descriptor < 0) {
        last_errno_ = errno;
        error = error_text("No se pudo abrir " + device.path, last_errno_);
        return false;
    }

    struct stat info {};
    if (::fstat(descriptor, &info) != 0) {
        last_errno_ = errno;
        error = error_text("No se pudo consultar " + device.path, last_errno_);
        ::close(descriptor);
        return false;
    }

    std::uint64_t size = 0;
    std::uint32_t sector = device.logical_sector != 0 ? device.logical_sector : 512;
    bool block = false;

    if (S_ISBLK(info.st_mode)) {
        std::uint64_t block_bytes = 0;
        int block_sector = 0;
        if (::ioctl(descriptor, BLKGETSIZE64, &block_bytes) != 0) {
            last_errno_ = errno;
            error = error_text("BLKGETSIZE64 falló para " + device.path,
                               last_errno_);
            ::close(descriptor);
            return false;
        }
        if (::ioctl(descriptor, BLKSSZGET, &block_sector) != 0) {
            last_errno_ = errno;
            error = error_text("BLKSSZGET falló para " + device.path,
                               last_errno_);
            ::close(descriptor);
            return false;
        }
        size = block_bytes;
        sector = static_cast<std::uint32_t>(block_sector);
        block = true;
    } else if (S_ISREG(info.st_mode)) {
        size = static_cast<std::uint64_t>(info.st_size);
    } else {
        last_errno_ = ENOTBLK;
        error = device.path + " no es un dispositivo de bloque ni un archivo.";
        ::close(descriptor);
        return false;
    }

    fd_ = descriptor;
    size_ = size;
    sector_size_ = sector;
    block_device_ = block;
    bytes_written_ = 0;
    read_offset_ = 0;
    return true;
}

bool PosixDeviceWriter::write(const std::byte* data, std::size_t bytes,
                              std::string& error) {
    error.clear();
    last_errno_ = 0;
    if (fd_ < 0) {
        error = "El dispositivo no está abierto.";
        last_errno_ = EBADF;
        return false;
    }
    if (bytes > size_ - bytes_written_) {
        error = "La escritura se sale de la capacidad del dispositivo.";
        last_errno_ = ENOSPC;
        return false;
    }

    const auto* cursor = reinterpret_cast<const std::uint8_t*>(data);
    std::size_t written = 0;
    while (written < bytes) {
        const ssize_t result = ::write(fd_, cursor + written, bytes - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            last_errno_ = errno;
            error = error_text("Falló la escritura en " +
                                   std::to_string(bytes_written_ + written) +
                                   " bytes",
                               last_errno_);
            return false;
        }
        if (result == 0) {
            error = "La escritura no avanzó.";
            last_errno_ = EIO;
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    bytes_written_ += bytes;
    return true;
}

bool PosixDeviceWriter::sync(std::string& error) {
    error.clear();
    last_errno_ = 0;
    if (fd_ < 0) {
        error = "El dispositivo no está abierto.";
        last_errno_ = EBADF;
        return false;
    }
    if (::fsync(fd_) != 0) {
        last_errno_ = errno;
        error = error_text("fsync falló", last_errno_);
        return false;
    }
    if (block_device_) {
        if (::ioctl(fd_, BLKFLSBUF) != 0 && errno != ENOTTY && errno != EINVAL) {
            last_errno_ = errno;
            error = error_text("BLKFLSBUF falló", last_errno_);
            return false;
        }
    }
    return true;
}

bool PosixDeviceWriter::read_back(std::byte* data, std::size_t bytes,
                                  std::string& error) {
    const bool ok = read_at(read_offset_, data, bytes, error);
    if (ok) {
        read_offset_ += bytes;
    }
    return ok;
}

bool PosixDeviceWriter::read_at(std::uint64_t offset, std::byte* data,
                                std::size_t bytes, std::string& error) {
    error.clear();
    last_errno_ = 0;
    if (fd_ < 0) {
        error = "El dispositivo no está abierto.";
        last_errno_ = EBADF;
        return false;
    }
    if (offset > size_ || bytes > size_ - offset) {
        error = "La lectura se sale de la capacidad del dispositivo.";
        last_errno_ = EINVAL;
        return false;
    }

    auto* cursor = reinterpret_cast<std::uint8_t*>(data);
    std::size_t read_bytes = 0;
    while (read_bytes < bytes) {
        const ssize_t result =
            ::pread(fd_, cursor + read_bytes, bytes - read_bytes,
                    static_cast<off_t>(offset + read_bytes));
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            last_errno_ = errno;
            error = error_text("Falló la lectura en " +
                                   std::to_string(offset + read_bytes) + " bytes",
                               last_errno_);
            return false;
        }
        if (result == 0) {
            error = "La lectura terminó antes de lo esperado.";
            last_errno_ = EIO;
            return false;
        }
        read_bytes += static_cast<std::size_t>(result);
    }
    return true;
}

void PosixDeviceWriter::close() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = -1;
    size_ = 0;
    bytes_written_ = 0;
    read_offset_ = 0;
    block_device_ = false;
}

} // namespace booteable
