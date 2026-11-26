#ifndef BOOTEABLE_UI_UIFORMAT_HPP
#define BOOTEABLE_UI_UIFORMAT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "domain/Types.hpp"

namespace booteable::ui {

// Severidad del mensaje de estado de la pantalla principal. El color no puede
// depender solo del texto: un error y una pista se ven distinto.
enum class StatusLevel {
    Hint,
    Info,
    Success,
    Warning,
    Error,
};

// Color del terminal (nombre aceptado por Guayaba) para una severidad.
std::string status_color(StatusLevel level);

// Formato de tamaños para la TUI: unidades binarias con un decimal a partir de
// MiB, y bytes exactos por debajo. Nunca imprime "0 B" para un valor no nulo.
std::string format_bytes(std::uint64_t bytes);

// Recorta un SHA-256 a un prefijo legible y agrega "..." cuando sobra.
std::string short_hash(const std::string& hash, std::size_t prefix = 12);

// Etiquetas en español para las etapas y los resultados del dominio.
std::string stage_label(PrepStage stage);
std::string outcome_label(PrepOutcome outcome);

// Color del terminal (nombre aceptado por Guayaba) para un resultado. Los
// estados incompletos o cancelados se muestran en ámbar, nunca en verde.
std::string outcome_color(PrepOutcome outcome);

std::string verdict_label(SafetyVerdict verdict);
std::string verdict_color(SafetyVerdict verdict);
std::string transport_label(DeviceTransport transport);

// Una línea por partición montada, o "Sin montajes" si no hay ninguna.
std::string describe_mounts(const DeviceInfo& device);

// Texto de la lista de dispositivos del panel izquierdo.
std::string device_row(const DeviceInfo& device);

// Estado de una etapa dentro de la checklist de progreso:
// "[x] hecha", "[>] en curso" o "[ ] pendiente".
std::string progress_step(PrepStage current, PrepStage step);

} // namespace booteable::ui

#endif // BOOTEABLE_UI_UIFORMAT_HPP
