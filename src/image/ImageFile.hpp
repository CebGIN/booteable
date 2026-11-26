#ifndef BOOTEABLE_IMAGE_IMAGEFILE_HPP
#define BOOTEABLE_IMAGE_IMAGEFILE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace booteable {

/*
    Archivo de imagen con acceso aleatorio. `create` usa ftruncate para que el
    archivo tenga el tamaño exacto del disco y sea disperso: solo las regiones
    que se escriben ocupan bloques. `sync` hace fsync antes de releer.
*/
class ImageFile {
public:
    ImageFile() = default;
    ~ImageFile();

    ImageFile(const ImageFile&) = delete;
    ImageFile& operator=(const ImageFile&) = delete;

    bool create(const std::filesystem::path& path, std::uint64_t size_bytes,
                std::string& error);

    bool open_read(const std::filesystem::path& path, std::string& error);

    bool write_at(std::uint64_t offset, const void* data, std::size_t length,
                  std::string& error);
    bool read_at(std::uint64_t offset, void* data, std::size_t length,
                 std::string& error) const;

    bool sync(std::string& error);
    bool close(std::string& error);

    int fd() const { return fd_; }
    std::uint64_t size() const { return size_; }
    bool is_open() const { return fd_ >= 0; }

private:
    int fd_ = -1;
    std::uint64_t size_ = 0;
};

// Error del sistema con contexto, para no perder errno en los mensajes.
std::string system_error_text(const std::string& action);

} // namespace booteable

#endif // BOOTEABLE_IMAGE_IMAGEFILE_HPP
