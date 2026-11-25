#ifndef BOOTEABLE_DEVICE_SAFETYPOLICY_HPP
#define BOOTEABLE_DEVICE_SAFETYPOLICY_HPP

#include <cstdint>

#include "domain/Types.hpp"

namespace booteable {

/*
    Política pura de seguridad. No lee /sys, no abre /dev y no toca hardware:
    recibe la foto del dispositivo y el contexto del sistema. Las reglas siguen
    Arquitectura §10.2 y se prueban con fixtures.
*/
class SafetyPolicy {
public:
    SafetyReport evaluate(const DeviceInfo& device,
                          const SystemContext& context) const;

    // Variante para el momento de escribir: un destino montado no se rechaza,
    // porque MountManager lo desmontará antes de abrir. El resto de reglas se
    // mantiene igual.
    SafetyReport evaluate_for_write(const DeviceInfo& device,
                                    const SystemContext& context) const;

    // Capacidad mínima para alojar la GPT, la ESP mínima del sector y la
    // alineación de 1 MiB. No confunde un disco pequeño con uno válido.
    static std::uint64_t minimum_device_bytes(std::uint32_t logical_sector);

private:
    SafetyReport evaluate_impl(const DeviceInfo& device,
                               const SystemContext& context,
                               bool allow_mounted) const;
};

} // namespace booteable

#endif // BOOTEABLE_DEVICE_SAFETYPOLICY_HPP
