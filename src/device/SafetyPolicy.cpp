#include "device/SafetyPolicy.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "device/DeviceIdentity.hpp"
#include "domain/Validation.hpp"

namespace booteable {
namespace {

constexpr std::uint64_t kGptEntriesBytes = 128ull * 128ull;

void reject(SafetyReport& report, std::string reason) {
    report.reasons.push_back(std::move(reason));
}

void warn(SafetyReport& report, std::string warning) {
    report.warnings.push_back(std::move(warning));
}

bool is_system_disk(const std::string& name, const SystemContext& context) {
    if (name.empty()) {
        return false;
    }
    return (!context.root_device.empty() && name == context.root_device) ||
           (!context.boot_device.empty() && name == context.boot_device);
}

std::string join(const std::vector<std::string>& values) {
    std::string result;
    for (const std::string& value : values) {
        if (!result.empty()) {
            result += ", ";
        }
        result += value;
    }
    return result;
}

} // namespace

std::uint64_t SafetyPolicy::minimum_device_bytes(
    std::uint32_t logical_sector) {
    const std::uint32_t sector =
        is_supported_sector(logical_sector) ? logical_sector : 512;
    const std::uint64_t entry_sectors =
        (kGptEntriesBytes + sector - 1) / sector;
    // 1 MiB de alineación + ESP mínima + GPT primaria + GPT de respaldo.
    return kMib + min_esp_bytes(sector) + 2ull * entry_sectors * sector;
}

SafetyReport SafetyPolicy::evaluate(const DeviceInfo& device,
                                    const SystemContext& context) const {
    return evaluate_impl(device, context, false);
}

SafetyReport SafetyPolicy::evaluate_for_write(
    const DeviceInfo& device, const SystemContext& context) const {
    return evaluate_impl(device, context, true);
}

SafetyReport SafetyPolicy::evaluate_impl(const DeviceInfo& device,
                                         const SystemContext& context,
                                         bool allow_mounted) const {
    SafetyReport report;

    if (device.path.empty()) {
        reject(report, "Destino inválido: falta la ruta del dispositivo.");
    }

    if (device.transport != DeviceTransport::Usb) {
        reject(report,
               "No es un dispositivo USB (transporte distinto de USB).");
    }
    if (!device.removable) {
        reject(report,
               "El dispositivo no es extraíble; puede confundirse con un disco "
               "fijo.");
    }

    const std::string name = device_basename(device.path);
    if (is_system_disk(name, context)) {
        reject(report,
               "Es el disco del sistema (contiene la raíz o /boot) y no se "
               "puede sobrescribir.");
    }

    if (device.serial.empty()) {
        reject(report,
               "El dispositivo no tiene número de serie estable; la identidad "
               "no es comprobable.");
    }
    if (device.by_id.empty()) {
        reject(report,
               "No se pudo resolver /dev/disk/by-id/usb-* para el dispositivo.");
    }
    if (device.read_only) {
        reject(report, "El medio está en solo lectura.");
    }
    if (!is_supported_sector(device.logical_sector)) {
        reject(report, "Sector lógico no soportado: " +
                           std::to_string(device.logical_sector) +
                           " bytes; se admiten 512 y 4096.");
    }
    if (device.size_bytes < minimum_device_bytes(device.logical_sector)) {
        reject(report,
               "Capacidad insuficiente: se necesitan al menos " +
                   std::to_string(minimum_device_bytes(device.logical_sector) /
                                  kMib) +
                   " MiB y el dispositivo reporta " +
                   std::to_string(device.size_bytes / kMib) + " MiB.");
    }
    if (!device.holders.empty()) {
        reject(report,
               "El dispositivo está en uso por otro subsistema (holders: " +
                   join(device.holders) + ").");
    }

    std::vector<std::string> mounted;
    for (const DevicePartition& partition : device.partitions) {
        if (!partition.mount_point.empty()) {
            mounted.push_back(partition.name + " en " + partition.mount_point);
        }
    }
    if (!mounted.empty()) {
        if (allow_mounted) {
            warn(report,
                 "El destino tiene particiones montadas (" + join(mounted) +
                     "); se desmontarán antes de abrir.");
        } else {
            reject(report,
                   "Tiene particiones montadas (" + join(mounted) +
                       "); hay que desmontarlas antes de escribir.");
        }
    }

    if (!device.partitions.empty() && mounted.empty()) {
        warn(report,
             "El dispositivo ya tiene particiones; se perderá todo su "
             "contenido.");
    }

    if (!report.reasons.empty()) {
        report.verdict = SafetyVerdict::Rejected;
    } else if (!report.warnings.empty()) {
        report.verdict = SafetyVerdict::AllowedWithWarning;
    } else {
        report.verdict = SafetyVerdict::Allowed;
    }
    return report;
}

} // namespace booteable
