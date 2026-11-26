#include "image/ImageFile.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/fs.h>

namespace booteable {

std::string system_error_text(const std::string& action) {
    return action + ": " + std::strerror(errno);
}

ImageFile::~ImageFile() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool ImageFile::create(const std::filesystem::path& path,
                       std::uint64_t size_bytes, std::string& error) {
    if (fd_ >= 0) {
        error = "El archivo de imagen ya está abierto.";
        return false;
    }
    if (size_bytes == 0) {
        error = "El tamaño de la imagen debe ser mayor que cero.";
        return false;
    }

    const int descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC,
                                  0644);
    if (descriptor < 0) {
        error = system_error_text("No se pudo crear la imagen");
        return false;
    }

    struct stat info {};
    if (::fstat(descriptor, &info) != 0) {
        error = system_error_text("No se pudo consultar la imagen");
        ::close(descriptor);
        return false;
    }
    if (!S_ISREG(info.st_mode)) {
        error = "La salida de build debe ser un archivo regular.";
        ::close(descriptor);
        return false;
    }

    if (::ftruncate(descriptor, static_cast<off_t>(size_bytes)) != 0) {
        error = system_error_text("No se pudo fijar el tamaño de la imagen");
        ::close(descriptor);
        return false;
    }

    fd_ = descriptor;
    size_ = size_bytes;
    return true;
}

bool ImageFile::open_read(const std::filesystem::path& path,
                          std::string& error) {
    if (fd_ >= 0) {
        error = "El archivo de imagen ya está abierto.";
        return false;
    }

    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        error = system_error_text("No se pudo abrir la imagen");
        return false;
    }

    struct stat info {};
    if (::fstat(descriptor, &info) != 0) {
        error = system_error_text("No se pudo consultar la imagen");
        ::close(descriptor);
        return false;
    }
    if (S_ISREG(info.st_mode)) {
        fd_ = descriptor;
        size_ = static_cast<std::uint64_t>(info.st_size);
        return true;
    }
    if (S_ISBLK(info.st_mode)) {
        std::uint64_t block_bytes = 0;
        if (::ioctl(descriptor, BLKGETSIZE64, &block_bytes) != 0) {
            error = system_error_text("No se pudo consultar el tamaño del bloque");
            ::close(descriptor);
            return false;
        }
        fd_ = descriptor;
        size_ = block_bytes;
        return true;
    }

    error = "La imagen no es un archivo regular ni un dispositivo de bloque.";
    ::close(descriptor);
    return false;
}

bool ImageFile::write_at(std::uint64_t offset, const void* data,
                         std::size_t length, std::string& error) {
    if (fd_ < 0) {
        error = "La imagen no está abierta para escritura.";
        return false;
    }
    if (offset > size_ || length > size_ - offset) {
        error = "La escritura se sale del tamaño de la imagen.";
        return false;
    }

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::size_t written = 0;
    while (written < length) {
        const ssize_t result =
            ::pwrite(fd_, bytes + written, length - written,
                     static_cast<off_t>(offset + written));
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            error = system_error_text("Falló la escritura de la imagen");
            return false;
        }
        if (result == 0) {
            error = "La escritura de la imagen no avanzó.";
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    return true;
}

bool ImageFile::read_at(std::uint64_t offset, void* data, std::size_t length,
                        std::string& error) const {
    if (fd_ < 0) {
        error = "La imagen no está abierta para lectura.";
        return false;
    }
    if (offset > size_ || length > size_ - offset) {
        error = "La lectura se sale del tamaño de la imagen.";
        return false;
    }

    auto* bytes = static_cast<std::uint8_t*>(data);
    std::size_t read_bytes = 0;
    while (read_bytes < length) {
        const ssize_t result =
            ::pread(fd_, bytes + read_bytes, length - read_bytes,
                    static_cast<off_t>(offset + read_bytes));
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            error = system_error_text("Falló la lectura de la imagen");
            return false;
        }
        if (result == 0) {
            error = "La lectura de la imagen terminó antes de lo esperado.";
            return false;
        }
        read_bytes += static_cast<std::size_t>(result);
    }
    return true;
}

bool ImageFile::sync(std::string& error) {
    if (fd_ < 0) {
        error = "La imagen no está abierta.";
        return false;
    }
    if (::fsync(fd_) != 0) {
        error = system_error_text("Falló fsync de la imagen");
        return false;
    }
    return true;
}

bool ImageFile::close(std::string& error) {
    if (fd_ < 0) {
        return true;
    }
    const int descriptor = fd_;
    fd_ = -1;
    if (::close(descriptor) != 0) {
        error = system_error_text("Falló el cierre de la imagen");
        return false;
    }
    return true;
}

} // namespace booteable
