#include "device/DeviceFlasher.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "image/ImageVerifier.hpp"

namespace booteable {
namespace {

constexpr std::uint64_t kChunkBytes = 1024ull * 1024ull;

PrepOutcome outcome_for_errno(int code) {
    switch (code) {
    case EACCES:
    case EPERM:
        return PrepOutcome::PermissionDenied;
    case ENODEV:
    case ENXIO:
        return PrepOutcome::Disconnected;
    case ENOSPC:
        return PrepOutcome::InsufficientSpace;
    default:
        return PrepOutcome::DeviceError;
    }
}

bool same_identity(const DeviceInfo& expected, const DeviceInfo& current) {
    return expected.path == current.path && expected.serial == current.serial &&
           expected.by_id == current.by_id &&
           expected.size_bytes == current.size_bytes &&
           expected.logical_sector == current.logical_sector;
}

} // namespace

PrepResult flash_image_to_device(const DeviceInfo& device,
                                 const std::filesystem::path& image,
                                 const ImageManifest& expected,
                                 IDeviceWriter& writer, MountManager& mounts,
                                 const FlashCallbacks& callbacks) {
    PrepResult result;
    result.image_sha256 = expected.manifest_sha256;

    const std::uint64_t total = expected.disk_bytes;
    const auto cancelled = [&callbacks]() {
        return callbacks.cancel != nullptr && callbacks.cancel->load();
    };
    const auto emit = [&callbacks, total](PrepStage stage, std::uint64_t done,
                                          const std::string& detail) {
        if (!callbacks.progress) {
            return;
        }
        PrepProgress progress;
        progress.stage = stage;
        progress.bytes_done = done;
        progress.bytes_total = total;
        progress.percent = total == 0
                               ? 0
                               : static_cast<int>(std::min<std::uint64_t>(
                                     100, done * 100 / total));
        progress.detail = detail;
        callbacks.progress(progress);
    };

    const auto fail = [&](PrepOutcome outcome, PrepStage stage,
                          std::string message, bool incomplete = false) {
        result.outcome = outcome;
        result.failed_stage = stage;
        result.message = std::move(message);
        if (incomplete && result.bytes_written > 0) {
            result.message += " El medio quedó incompleto.";
        }
        result.verified = false;
        writer.close();
        PrepStage terminal = PrepStage::Failed;
        if (outcome == PrepOutcome::Incomplete) {
            terminal = PrepStage::Incomplete;
        } else if (outcome == PrepOutcome::Cancelled) {
            terminal = PrepStage::Cancelled;
        }
        emit(terminal, result.bytes_written, result.message);
        return result;
    };

    if (cancelled()) {
        return fail(PrepOutcome::Cancelled, PrepStage::Validating,
                    "Preparación cancelada antes de empezar.");
    }
    if (expected.disk_bytes == 0 || expected.regions.empty()) {
        return fail(PrepOutcome::BuildError, PrepStage::Validating,
                    "El manifiesto de la imagen está vacío o incompleto.");
    }
    if (device.size_bytes != expected.disk_bytes) {
        return fail(PrepOutcome::InvalidTarget, PrepStage::Validating,
                    "La imagen mide " + std::to_string(expected.disk_bytes) +
                        " bytes y el dispositivo " +
                        std::to_string(device.size_bytes) +
                        " bytes; deben coincidir.");
    }
    if (device.logical_sector != 512 && device.logical_sector != 4096) {
        return fail(PrepOutcome::InvalidTarget, PrepStage::Validating,
                    "Sector lógico del dispositivo no soportado: " +
                        std::to_string(device.logical_sector) + " bytes.");
    }

    std::uint32_t image_sector = 0;
    std::string error;
    if (!detect_image_sector_size(image, image_sector, error)) {
        return fail(PrepOutcome::BuildError, PrepStage::Validating, error);
    }
    if (image_sector != device.logical_sector) {
        return fail(PrepOutcome::InvalidTarget, PrepStage::Validating,
                    "La imagen usa sectores de " +
                        std::to_string(image_sector) +
                        " bytes y el dispositivo de " +
                        std::to_string(device.logical_sector) +
                        "; volvé a construirla con --sector-size.");
    }

    if (callbacks.refresh) {
        const std::optional<DeviceInfo> current =
            callbacks.refresh(device.path);
        if (!current) {
            return fail(PrepOutcome::Disconnected, PrepStage::Validating,
                        "El dispositivo desapareció antes de escribir.");
        }
        if (!same_identity(device, *current)) {
            return fail(PrepOutcome::Disconnected, PrepStage::Validating,
                        "La identidad del dispositivo cambió antes de escribir.");
        }
    }

    emit(PrepStage::Validating, 0, "Identidad y geometría validadas");

    if (cancelled()) {
        return fail(PrepOutcome::Cancelled, PrepStage::Unmounting,
                    "Preparación cancelada antes de desmontar.");
    }
    emit(PrepStage::Unmounting, 0, "Desmontando particiones del destino");
    std::vector<std::string> unmounted;
    if (!mounts.unmount_all(device, unmounted, error)) {
        return fail(PrepOutcome::UnmountError, PrepStage::Unmounting, error);
    }
    emit(PrepStage::Unmounting, 0,
         unmounted.empty() ? "El destino no tenía montajes"
                           : "Desmontado: " + unmounted.front());

    if (cancelled()) {
        return fail(PrepOutcome::Cancelled, PrepStage::Opening,
                    "Preparación cancelada antes de abrir.");
    }
    if (callbacks.refresh) {
        const std::optional<DeviceInfo> current =
            callbacks.refresh(device.path);
        if (!current) {
            return fail(PrepOutcome::Disconnected, PrepStage::Opening,
                        "El dispositivo desapareció después de desmontar.");
        }
        if (!same_identity(device, *current)) {
            return fail(PrepOutcome::Disconnected, PrepStage::Opening,
                        "La identidad del dispositivo cambió después de "
                        "desmontar.");
        }
    }

    emit(PrepStage::Opening, 0, "Abriendo el dispositivo");
    if (!writer.open(device, error)) {
        return fail(outcome_for_errno(writer.last_errno()), PrepStage::Opening,
                    error);
    }
    if (writer.size() != 0 && writer.size() != expected.disk_bytes) {
        return fail(PrepOutcome::InvalidTarget, PrepStage::Opening,
                    "El destino abierto mide " +
                        std::to_string(writer.size()) + " bytes y la imagen " +
                        std::to_string(expected.disk_bytes) + ".");
    }
    if (writer.sector_size() != 0 &&
        writer.sector_size() != device.logical_sector) {
        return fail(PrepOutcome::InvalidTarget, PrepStage::Opening,
                    "El sector real del destino no coincide con el declarado.");
    }

    std::ifstream input(image, std::ios::binary);
    if (!input) {
        return fail(PrepOutcome::BuildError, PrepStage::Writing,
                    "No se pudo abrir la imagen para leerla: " + image.string());
    }

    emit(PrepStage::Writing, 0, "Escribiendo la imagen");
    std::vector<char> buffer(static_cast<std::size_t>(kChunkBytes));
    std::uint64_t offset = 0;
    while (offset < expected.disk_bytes) {
        if (cancelled()) {
            std::string ignored;
            result.synchronized = writer.sync(ignored);
            return fail(PrepOutcome::Incomplete, PrepStage::Writing,
                        "Preparación cancelada durante la escritura.", true);
        }

        const std::size_t take = static_cast<std::size_t>(
            std::min<std::uint64_t>(buffer.size(), expected.disk_bytes - offset));
        input.read(buffer.data(), static_cast<std::streamsize>(take));
        if (input.gcount() != static_cast<std::streamsize>(take)) {
            return fail(PrepOutcome::BuildError, PrepStage::Writing,
                        "La imagen terminó antes de lo esperado.", true);
        }

        if (!writer.write(reinterpret_cast<const std::byte*>(buffer.data()),
                          take, error)) {
            const PrepOutcome outcome = outcome_for_errno(writer.last_errno());
            std::string ignored;
            result.synchronized = writer.sync(ignored);
            result.bytes_written = writer.bytes_written() != 0
                                       ? writer.bytes_written()
                                       : offset;
            return fail(outcome, PrepStage::Writing, error, true);
        }
        offset += take;
        result.bytes_written = writer.bytes_written() != 0
                                   ? writer.bytes_written()
                                   : offset;
        emit(PrepStage::Writing, offset, "Escribiendo la imagen");
    }

    if (cancelled()) {
        std::string ignored;
        result.synchronized = writer.sync(ignored);
        return fail(PrepOutcome::Incomplete, PrepStage::Syncing,
                    "Preparación cancelada antes de sincronizar.", true);
    }

    emit(PrepStage::Syncing, expected.disk_bytes, "Sincronizando el medio");
    if (!writer.sync(error)) {
        const PrepOutcome outcome = outcome_for_errno(writer.last_errno());
        return fail(outcome, PrepStage::Syncing, error, true);
    }
    result.synchronized = true;
    writer.close();

    if (cancelled()) {
        return fail(PrepOutcome::Incomplete, PrepStage::Verifying,
                    "Preparación cancelada antes de verificar.", true);
    }

    emit(PrepStage::Verifying, expected.disk_bytes,
         "Releyendo y comparando el manifiesto");
    ImageManifest actual;
    ImageVerifier verifier;
    if (!verifier.verify_against(device.path, expected, actual, error)) {
        return fail(PrepOutcome::VerificationFailed, PrepStage::Verifying,
                    "La relectura no coincide: " + error, true);
    }
    result.device_sha256 = actual.manifest_sha256;

    if (callbacks.refresh) {
        const std::optional<DeviceInfo> current =
            callbacks.refresh(device.path);
        if (!current) {
            return fail(PrepOutcome::Disconnected, PrepStage::Done,
                        "El dispositivo desapareció después de verificar.");
        }
        if (!same_identity(device, *current)) {
            return fail(PrepOutcome::Disconnected, PrepStage::Done,
                        "La identidad del dispositivo cambió después de "
                        "verificar.");
        }
    }

    result.outcome = PrepOutcome::Verified;
    result.failed_stage = PrepStage::Idle;
    result.message = "La imagen se escribió, sincronizó y verificó.";
    result.bytes_written = expected.disk_bytes;
    result.synchronized = true;
    result.verified = true;
    emit(PrepStage::Done, expected.disk_bytes, result.message);
    return result;
}

} // namespace booteable
