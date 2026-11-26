#include "app/PrepRunner.hpp"

#include <algorithm>
#include <cerrno>
#include <exception>
#include <string>
#include <utility>

#include <unistd.h>

#include "EmbeddedBootImage.hpp"
#include "device/DeviceFlasher.hpp"
#include "device/MountManager.hpp"
#include "device/PosixDeviceWriter.hpp"
#include "device/SafetyPolicy.hpp"
#include "domain/Validation.hpp"

namespace booteable {
namespace {

class ScopedImage {
public:
    ScopedImage(std::filesystem::path path, bool owned)
        : path_(std::move(path)), owned_(owned) {}

    ~ScopedImage() {
        if (!owned_ || path_.empty()) {
            return;
        }
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    bool owned_ = false;
};

std::filesystem::path temporary_image_path() {
    static std::atomic<unsigned> counter{0};
    const unsigned sequence = counter.fetch_add(1);
    return std::filesystem::temp_directory_path() /
           ("booteable-" + std::to_string(static_cast<long>(::getpid())) + "-" +
            std::to_string(sequence) + ".img");
}

int mapped_percent(const PrepProgress& progress) {
    switch (progress.stage) {
    case PrepStage::Building:
        return progress.percent;
    case PrepStage::Validating:
        return 15;
    case PrepStage::Unmounting:
        return 18;
    case PrepStage::Opening:
        return 20;
    case PrepStage::Writing:
        return 20 + progress.percent * 72 / 100;
    case PrepStage::Syncing:
        return 94;
    case PrepStage::Verifying:
        return 97;
    case PrepStage::Done:
        return 100;
    default:
        return progress.percent;
    }
}

PrepResult failure(PrepOutcome outcome, PrepStage stage, std::string message) {
    PrepResult result;
    result.outcome = outcome;
    result.failed_stage = stage;
    result.message = std::move(message);
    return result;
}

} // namespace

PrepRunner::PrepRunner(IBlockDeviceSource& source) : source_(source) {}

ImageBuildResult PrepRunner::build_image(const PrepConfig& config,
                                         const std::filesystem::path& output) {
    ImageBuildResult result;

    const ValidationReport validation = validate(config, kBootEfiSize);
    if (!validation.ok()) {
        result.error = validation.issues.front().message;
        return result;
    }

    BootImageSpec spec;
    spec.disk_bytes = config.device.size_bytes;
    spec.esp_bytes = config.esp_bytes;
    spec.volume_label = config.volume_label;
    spec.logical_sector = config.device.logical_sector;

    result = build_boot_image(output, spec, kBootEfi, kBootEfiSize);
    if (result.ok) {
        result.esp_bytes = result.layout.esp_sectors * result.layout.sector_size;
    }
    return result;
}

PrepResult PrepRunner::run(
    const PrepConfig& config, const std::atomic_bool& cancel,
    const std::function<void(const PrepProgress&)>& progress,
    WaitBeforeWrite wait_before_write) {
    const auto emit = [&progress](PrepStage stage, int percent,
                                  const std::string& detail) {
        if (!progress) {
            return;
        }
        PrepProgress value;
        value.stage = stage;
        value.percent = percent;
        value.detail = detail;
        progress(value);
    };

    if (cancel.load()) {
        return failure(PrepOutcome::Cancelled, PrepStage::Validating,
                       "Preparación cancelada antes de empezar.");
    }

    std::filesystem::path image;
    try {
        image = config.image_path.empty() ? temporary_image_path()
                                          : std::filesystem::path(config.image_path);
    } catch (const std::exception& error) {
        return failure(PrepOutcome::BuildError, PrepStage::Building,
                       std::string("No se pudo resolver el archivo temporal: ") +
                           error.what());
    }
    ScopedImage scoped_image(image, config.image_path.empty());

    try {
        const ValidationReport validation = validate(config, kBootEfiSize);
        if (!validation.ok()) {
            const std::string message = validation.issues.front().message;
            emit(PrepStage::Failed, 0, message);
            return failure(PrepOutcome::InvalidTarget, PrepStage::Validating,
                           message);
        }

        const SafetyPolicy policy;
        const SafetyReport safety =
            policy.evaluate_for_write(config.device, source_.system_context());
        if (safety.verdict == SafetyVerdict::Rejected) {
            const std::string message =
                safety.reasons.empty() ? "El destino fue rechazado."
                                       : safety.reasons.front();
            emit(PrepStage::Failed, 0, message);
            return failure(PrepOutcome::InvalidTarget, PrepStage::Validating,
                           message);
        }

        emit(PrepStage::Building, 5, "Construyendo la imagen GPT/FAT32");
        const ImageBuildResult built = build_image(config, image);
        if (!built.ok) {
            emit(PrepStage::Failed, 5, built.error);
            return failure(PrepOutcome::BuildError, PrepStage::Building,
                           built.error);
        }
        emit(PrepStage::Building, 15,
             "Imagen construida; ESP de " +
                 std::to_string(built.esp_bytes / kMib) + " MiB");

        if (cancel.load()) {
            emit(PrepStage::Cancelled, 15, "Cancelado antes de escribir");
            return failure(PrepOutcome::Cancelled, PrepStage::Building,
                           "Preparación cancelada antes de escribir.");
        }

        // Único punto de pausa real: la imagen ya está construida y verificada,
        // la USB todavía no se abrió. El worker espera a que la UI confirme.
        if (wait_before_write && !wait_before_write()) {
            emit(PrepStage::Cancelled, 15, "Cancelado antes de escribir");
            return failure(PrepOutcome::Cancelled, PrepStage::Building,
                           "Preparación cancelada antes de escribir.");
        }

        MountManager mounts;
        PosixDeviceWriter writer;
        FlashCallbacks callbacks;
        callbacks.cancel = &cancel;
        callbacks.refresh = [this](const std::string& path) {
            return source_.refresh(path);
        };
        callbacks.progress = [&emit](const PrepProgress& value) {
            emit(value.stage, mapped_percent(value), value.detail);
        };

        return flash_image_to_device(config.device, image, built.manifest,
                                     writer, mounts, callbacks);
    } catch (const std::exception& error) {
        const std::string message =
            std::string("Excepción durante la preparación: ") + error.what();
        emit(PrepStage::Failed, 0, message);
        return failure(PrepOutcome::DeviceError, PrepStage::Writing, message);
    } catch (...) {
        const std::string message =
            "Excepción desconocida durante la preparación.";
        emit(PrepStage::Failed, 0, message);
        return failure(PrepOutcome::DeviceError, PrepStage::Writing, message);
    }
}

} // namespace booteable
