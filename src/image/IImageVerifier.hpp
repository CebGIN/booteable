#ifndef BOOTEABLE_IMAGE_IIMAGEVERIFIER_HPP
#define BOOTEABLE_IMAGE_IIMAGEVERIFIER_HPP

#include <filesystem>
#include <string>

#include "domain/Types.hpp"

namespace booteable {

// Verifica una imagen construida y devuelve su manifiesto. No escribe nada.
class IImageVerifier {
public:
    virtual ~IImageVerifier() = default;

    virtual bool verify(const std::filesystem::path& image,
                        ImageManifest& out, std::string& error) const = 0;
};

} // namespace booteable

#endif // BOOTEABLE_IMAGE_IIMAGEVERIFIER_HPP
