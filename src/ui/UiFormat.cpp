#include "ui/UiFormat.hpp"

#include <iomanip>
#include <sstream>

namespace booteable::ui {
namespace {

constexpr std::uint64_t kKib = 1024ull;
constexpr std::uint64_t kMib = 1024ull * kKib;
constexpr std::uint64_t kGib = 1024ull * kMib;

std::string with_unit(double value, const char* unit) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value << ' ' << unit;
    return stream.str();
}

int stage_order(PrepStage stage) {
    switch (stage) {
    case PrepStage::Idle:
        return -1;
    case PrepStage::Enumerating:
        return 0;
    case PrepStage::Validating:
        return 1;
    case PrepStage::Confirming:
        return 2;
    case PrepStage::Building:
        return 3;
    case PrepStage::Unmounting:
        return 4;
    case PrepStage::Opening:
        return 5;
    case PrepStage::Writing:
        return 6;
    case PrepStage::Syncing:
        return 7;
    case PrepStage::Verifying:
        return 8;
    case PrepStage::Done:
        return 9;
    case PrepStage::Failed:
    case PrepStage::Cancelled:
    case PrepStage::Incomplete:
        return -2;
    }
    return -2;
}

} // namespace

std::string status_color(StatusLevel level) {
    switch (level) {
    case StatusLevel::Hint:
        return "BRIGHT_BLACK";
    case StatusLevel::Info:
        return "BRIGHT_WHITE";
    case StatusLevel::Success:
        return "BRIGHT_GREEN";
    case StatusLevel::Warning:
        return "BRIGHT_YELLOW";
    case StatusLevel::Error:
        return "BRIGHT_RED";
    }
    return "BRIGHT_BLACK";
}

std::string format_bytes(std::uint64_t bytes) {
    if (bytes >= kGib) {
        return with_unit(static_cast<double>(bytes) / static_cast<double>(kGib),
                         "GiB");
    }
    if (bytes >= kMib) {
        return with_unit(static_cast<double>(bytes) / static_cast<double>(kMib),
                         "MiB");
    }
    if (bytes >= kKib) {
        return with_unit(static_cast<double>(bytes) / static_cast<double>(kKib),
                         "KiB");
    }
    return std::to_string(bytes) + " B";
}

std::string short_hash(const std::string& hash, std::size_t prefix) {
    if (hash.empty()) {
        return "(sin hash)";
    }
    if (hash.size() <= prefix) {
        return hash;
    }
    return hash.substr(0, prefix) + "...";
}

std::string stage_label(PrepStage stage) {
    switch (stage) {
    case PrepStage::Idle:
        return "En espera";
    case PrepStage::Enumerating:
        return "Enumerando dispositivos";
    case PrepStage::Validating:
        return "Validando destino e imagen";
    case PrepStage::Confirming:
        return "Esperando confirmación";
    case PrepStage::Building:
        return "Construyendo la imagen";
    case PrepStage::Unmounting:
        return "Desmontando particiones";
    case PrepStage::Opening:
        return "Abriendo el dispositivo";
    case PrepStage::Writing:
        return "Escribiendo la imagen";
    case PrepStage::Syncing:
        return "Sincronizando el medio";
    case PrepStage::Verifying:
        return "Verificando la relectura";
    case PrepStage::Done:
        return "Completado";
    case PrepStage::Failed:
        return "Falló";
    case PrepStage::Cancelled:
        return "Cancelado";
    case PrepStage::Incomplete:
        return "Incompleto";
    }
    return "Desconocido";
}

std::string outcome_label(PrepOutcome outcome) {
    switch (outcome) {
    case PrepOutcome::None:
        return "SIN RESULTADO";
    case PrepOutcome::Verified:
        return "VERIFICADO";
    case PrepOutcome::VerificationFailed:
        return "VERIFICACIÓN FALLIDA";
    case PrepOutcome::Cancelled:
        return "CANCELADO";
    case PrepOutcome::Incomplete:
        return "INCOMPLETO";
    case PrepOutcome::PermissionDenied:
        return "SIN PERMISOS";
    case PrepOutcome::InvalidTarget:
        return "DESTINO INVÁLIDO";
    case PrepOutcome::Disconnected:
        return "USB DESCONECTADA";
    case PrepOutcome::InsufficientSpace:
        return "ESPACIO INSUFICIENTE";
    case PrepOutcome::BuildError:
        return "ERROR DE IMAGEN";
    case PrepOutcome::DeviceError:
        return "ERROR DEL DISPOSITIVO";
    case PrepOutcome::UnmountError:
        return "ERROR AL DESMONTAR";
    }
    return "DESCONOCIDO";
}

std::string outcome_color(PrepOutcome outcome) {
    switch (outcome) {
    case PrepOutcome::Verified:
        return "BRIGHT_GREEN";
    case PrepOutcome::Cancelled:
    case PrepOutcome::Incomplete:
        return "BRIGHT_YELLOW";
    case PrepOutcome::None:
        return "BRIGHT_WHITE";
    default:
        return "BRIGHT_RED";
    }
}

std::string verdict_label(SafetyVerdict verdict) {
    switch (verdict) {
    case SafetyVerdict::Allowed:
        return "APTO";
    case SafetyVerdict::AllowedWithWarning:
        return "APTO CON ADVERTENCIA";
    case SafetyVerdict::Rejected:
        return "RECHAZADO";
    }
    return "DESCONOCIDO";
}

std::string verdict_color(SafetyVerdict verdict) {
    switch (verdict) {
    case SafetyVerdict::Allowed:
        return "BRIGHT_GREEN";
    case SafetyVerdict::AllowedWithWarning:
        return "BRIGHT_YELLOW";
    case SafetyVerdict::Rejected:
        return "BRIGHT_RED";
    }
    return "BRIGHT_WHITE";
}

std::string transport_label(DeviceTransport transport) {
    switch (transport) {
    case DeviceTransport::Usb:
        return "USB";
    case DeviceTransport::Sata:
        return "SATA";
    case DeviceTransport::Nvme:
        return "NVMe";
    case DeviceTransport::Mmc:
        return "MMC";
    case DeviceTransport::Virtual:
        return "Virtual";
    case DeviceTransport::Unknown:
        return "Desconocido";
    }
    return "Desconocido";
}

std::string describe_mounts(const DeviceInfo& device) {
    std::string text;
    for (const DevicePartition& partition : device.partitions) {
        if (partition.mount_point.empty()) {
            continue;
        }
        if (!text.empty()) {
            text += ", ";
        }
        text += partition.mount_point + " (" + partition.name + ")";
    }
    return text.empty() ? "Sin montajes" : text;
}

std::string device_row(const DeviceInfo& device) {
    std::string text = device.path;
    if (!device.model.empty()) {
        text += "  " + device.model;
    }
    text += "  " + format_bytes(device.size_bytes);
    text += "  [" + transport_label(device.transport) + "]";
    return text;
}

std::string progress_step(PrepStage current, PrepStage step) {
    const int now = stage_order(current);
    const int target = stage_order(step);
    if (now == -1) {
        return "[ ] " + stage_label(step);
    }
    if (now >= 9) {
        return "[x] " + stage_label(step);
    }
    if (now < 0) {
        return "[ ] " + stage_label(step);
    }
    if (target < now) {
        return "[x] " + stage_label(step);
    }
    if (target == now) {
        return "[>] " + stage_label(step);
    }
    return "[ ] " + stage_label(step);
}

} // namespace booteable::ui
