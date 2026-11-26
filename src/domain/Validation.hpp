#ifndef BOOTEABLE_DOMAIN_VALIDATION_HPP
#define BOOTEABLE_DOMAIN_VALIDATION_HPP

#include <cstdint>

#include "domain/Types.hpp"

namespace booteable {

// Tamaño de sector lógico que el proyecto sabe formatear.
bool is_supported_sector(std::uint32_t logical_sector);

// Mínimo de ESP que permite una FAT32 válida con ese sector (65 525 clústeres
// como mínimo y un sector por clúster).
std::uint64_t min_esp_bytes(std::uint32_t logical_sector);

// Valor por defecto cuando PrepConfig::esp_bytes es 0.
std::uint64_t default_esp_bytes(std::uint32_t logical_sector);

// Tamaño efectivo de la ESP: el pedido o el valor por defecto del sector.
std::uint64_t effective_esp_bytes(const PrepConfig& config);

// Reglas de dominio que no dependen del binario EFI incrustado.
ValidationReport validate_config(const PrepConfig& config);

// Comprueba que el binario EFI entra en la ESP efectiva.
ValidationReport validate_efi_size(const PrepConfig& config,
                                   std::uint64_t efi_bytes);

// Atajo para los adaptadores: valida configuración y tamaño del EFI.
ValidationReport validate(const PrepConfig& config, std::uint64_t efi_bytes);

} // namespace booteable

#endif // BOOTEABLE_DOMAIN_VALIDATION_HPP
