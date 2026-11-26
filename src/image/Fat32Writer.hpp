#ifndef BOOTEABLE_IMAGE_FAT32WRITER_HPP
#define BOOTEABLE_IMAGE_FAT32WRITER_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "domain/Types.hpp"

namespace booteable {

/*
    Callback de escritura dentro de la ESP. `offset` es relativo al inicio de la
    ESP y las llamadas llegan en orden ascendente. El constructor de la imagen
    lo usa para escribir en el archivo y hashear los huecos de ceros.
*/
using EspSink = std::function<bool(std::uint64_t offset, const void* data,
                                   std::size_t length, std::string& error)>;

/*
    Escribe una ESP FAT32 con el árbol EFI/BOOT/BOOTX64.EFI, sin nombres largos
    y con fechas fijas para que la imagen sea determinista. No asigna memoria
    proporcional al tamaño de la ESP: solo la FAT y los clústeres del árbol.
*/
bool write_fat32(const ImageLayout& layout, const std::string& volume_label,
                 const unsigned char* efi, std::size_t efi_size,
                 const EspSink& sink, std::string& error);

} // namespace booteable

#endif // BOOTEABLE_IMAGE_FAT32WRITER_HPP
