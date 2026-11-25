#include "device/FaultyDeviceWriter.hpp"

#include <cerrno>
#include <cstring>

namespace booteable {

FaultyDeviceWriter::FaultyDeviceWriter(IDeviceWriter& inner) : inner_(inner) {}

void FaultyDeviceWriter::fail_after(std::uint64_t after_bytes,
                                    int error_number) {
    fail_after_ = after_bytes;
    write_error_ = error_number != 0 ? error_number : EIO;
}

void FaultyDeviceWriter::fail_sync(int error_number) {
    sync_error_ = error_number != 0 ? error_number : EIO;
}

bool FaultyDeviceWriter::open(const DeviceInfo& device, std::string& error) {
    bytes_written_ = 0;
    last_errno_ = 0;
    return inner_.open(device, error);
}

bool FaultyDeviceWriter::write(const std::byte* data, std::size_t bytes,
                               std::string& error) {
    error.clear();
    last_errno_ = 0;

    if (fail_after_ != 0 && bytes_written_ >= fail_after_) {
        last_errno_ = write_error_;
        error = "Fallo inyectado antes de escribir: " +
                std::string(std::strerror(write_error_));
        return false;
    }

    std::size_t allowed = bytes;
    if (fail_after_ != 0 && bytes_written_ + bytes > fail_after_) {
        allowed = static_cast<std::size_t>(fail_after_ - bytes_written_);
    }

    if (allowed > 0) {
        if (!inner_.write(data, allowed, error)) {
            last_errno_ = inner_.last_errno();
            return false;
        }
        bytes_written_ += allowed;
    }

    if (allowed < bytes) {
        last_errno_ = write_error_;
        error = "Fallo inyectado después de " +
                std::to_string(bytes_written_) + " bytes: " +
                std::strerror(write_error_);
        return false;
    }
    return true;
}

bool FaultyDeviceWriter::sync(std::string& error) {
    error.clear();
    if (sync_error_ != 0) {
        last_errno_ = sync_error_;
        error = "Fallo inyectado en fsync: " +
                std::string(std::strerror(sync_error_));
        return false;
    }
    return inner_.sync(error);
}

bool FaultyDeviceWriter::read_back(std::byte* data, std::size_t bytes,
                                   std::string& error) {
    return inner_.read_back(data, bytes, error);
}

bool FaultyDeviceWriter::read_at(std::uint64_t offset, std::byte* data,
                                 std::size_t bytes, std::string& error) {
    return inner_.read_at(offset, data, bytes, error);
}

void FaultyDeviceWriter::close() {
    inner_.close();
}

int FaultyDeviceWriter::last_errno() const {
    return last_errno_ != 0 ? last_errno_ : inner_.last_errno();
}

} // namespace booteable
