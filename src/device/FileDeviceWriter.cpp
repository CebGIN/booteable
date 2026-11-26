#include "device/FileDeviceWriter.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace booteable {

FileDeviceWriter::FileDeviceWriter(bool create, bool truncate)
    : create_(create), truncate_(truncate) {}

bool FileDeviceWriter::open(const DeviceInfo& device, std::string& error) {
    error.clear();
    if (create_ || truncate_) {
        int flags = O_RDWR | O_CLOEXEC;
        if (create_) {
            flags |= O_CREAT;
        }
        const int descriptor = ::open(device.path.c_str(), flags, 0644);
        if (descriptor < 0) {
            error = "No se pudo preparar el archivo " + device.path + ": " +
                    std::strerror(errno);
            return false;
        }
        if (truncate_ && device.size_bytes > 0) {
            if (::ftruncate(descriptor,
                            static_cast<off_t>(device.size_bytes)) != 0) {
                error = "No se pudo fijar el tamaño de " + device.path + ": " +
                        std::strerror(errno);
                ::close(descriptor);
                return false;
            }
        }
        ::close(descriptor);
    }
    return PosixDeviceWriter::open(device, error);
}

} // namespace booteable
