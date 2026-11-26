#include "app/PrepController.hpp"

#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#include <unistd.h>

#include "EmbeddedBootImage.hpp"
#include "domain/Validation.hpp"

namespace booteable {
namespace {

PrepStage terminal_stage(PrepOutcome outcome) {
    switch (outcome) {
    case PrepOutcome::Verified:
        return PrepStage::Done;
    case PrepOutcome::Cancelled:
        return PrepStage::Cancelled;
    case PrepOutcome::Incomplete:
        return PrepStage::Incomplete;
    case PrepOutcome::None:
        return PrepStage::Idle;
    default:
        return PrepStage::Failed;
    }
}

} // namespace

PrepController::PrepController(IBlockDeviceSource& source, IPrepRunner& runner,
                               SafetyPolicy policy, PrivilegeCheck can_write)
    : source_(source),
      runner_(runner),
      policy_(std::move(policy)),
      can_write_(std::move(can_write)) {}

PrepController::~PrepController() {
    cancel_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool PrepController::default_can_write() {
    return ::geteuid() == 0;
}

std::vector<DeviceInfo> PrepController::devices() {
    return source_.list();
}

SafetyReport PrepController::safety(const DeviceInfo& device) {
    return policy_.evaluate_for_write(device, source_.system_context());
}

ValidationReport PrepController::validate(const PrepConfig& config) {
    ValidationReport report = booteable::validate(config, kBootEfiSize);

    if (!can_write()) {
        report.issues.push_back(
            ValidationIssue{"privileges", write_block_reason()});
    }

    const SafetyReport safety_report =
        policy_.evaluate_for_write(config.device, source_.system_context());
    if (safety_report.verdict == SafetyVerdict::Rejected) {
        for (const std::string& reason : safety_report.reasons) {
            report.issues.push_back(ValidationIssue{"safety", reason});
        }
    }
    return report;
}

void PrepController::publish_failure(PrepOutcome outcome, PrepStage stage,
                                     const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = PrepResult{};
    result_.outcome = outcome;
    result_.failed_stage = stage;
    result_.message = message;
    progress_.stage = stage == PrepStage::Validating ? PrepStage::Failed : stage;
    progress_.detail = message;
}

bool PrepController::start(const PrepConfig& config) {
    if (running_.load()) {
        return false;
    }
    if (worker_.joinable()) {
        worker_.join();
    }

    if (!can_write()) {
        publish_failure(PrepOutcome::PermissionDenied, PrepStage::Opening,
                        write_block_reason());
        return false;
    }
    if (!config.overwrite_confirmed) {
        publish_failure(PrepOutcome::InvalidTarget, PrepStage::Validating,
                        "Falta la casilla de confirmación de sobrescritura.");
        return false;
    }

    const SafetyReport safety_report =
        policy_.evaluate_for_write(config.device, source_.system_context());
    if (safety_report.verdict == SafetyVerdict::Rejected) {
        publish_failure(PrepOutcome::InvalidTarget, PrepStage::Validating,
                        safety_report.reasons.empty()
                            ? "El destino fue rechazado por la política."
                            : safety_report.reasons.front());
        return false;
    }

    const ValidationReport validation =
        booteable::validate(config, kBootEfiSize);
    if (!validation.ok()) {
        publish_failure(PrepOutcome::InvalidTarget, PrepStage::Validating,
                        validation.issues.front().message);
        return false;
    }

    cancel_.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_ = PrepProgress{};
        progress_.stage = PrepStage::Validating;
        progress_.bytes_total = config.device.size_bytes;
        progress_.detail = "Preparando la escritura";
        result_ = PrepResult{};
    }

    running_.store(true);
    try {
        worker_ = std::thread([this, config]() {
            PrepResult result;
            try {
                result = runner_.run(
                    config, cancel_,
                    [this, target_bytes = config.device.size_bytes](
                        const PrepProgress& value) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        progress_ = value;
                        if (progress_.bytes_total == 0) {
                            progress_.bytes_total = target_bytes;
                        }
                    });
            } catch (const std::exception& error) {
                result = PrepResult{};
                result.outcome = PrepOutcome::DeviceError;
                result.failed_stage = PrepStage::Writing;
                result.message =
                    std::string("Excepción durante la preparación: ") +
                    error.what();
            } catch (...) {
                result = PrepResult{};
                result.outcome = PrepOutcome::DeviceError;
                result.failed_stage = PrepStage::Writing;
                result.message =
                    "Excepción desconocida durante la preparación.";
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                result_ = result;
                progress_.stage = terminal_stage(result.outcome);
                progress_.detail = result.message;
                if (progress_.bytes_total == 0) {
                    progress_.bytes_total = config.device.size_bytes;
                }
                if (result.outcome == PrepOutcome::Verified) {
                    progress_.percent = 100;
                    progress_.bytes_done = progress_.bytes_total;
                } else if (progress_.bytes_done == 0) {
                    progress_.bytes_done = result.bytes_written;
                }
            }
            cancel_.store(false);
            running_.store(false);
        });
    } catch (const std::exception& error) {
        running_.store(false);
        publish_failure(PrepOutcome::DeviceError, PrepStage::Opening,
                        std::string("No se pudo iniciar el hilo: ") +
                            error.what());
        return false;
    }

    return true;
}

void PrepController::cancel() {
    if (running_.load()) {
        cancel_.store(true);
    }
}

PrepProgress PrepController::progress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

PrepResult PrepController::result() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return result_;
}

bool PrepController::running() const {
    return running_.load();
}

bool PrepController::can_write() const {
    return can_write_ ? can_write_() : true;
}

std::string PrepController::write_block_reason() const {
    if (can_write()) {
        return {};
    }
    return "Sin permisos para escribir dispositivos. Ejecutá la TUI con sudo.";
}

bool PrepController::export_image(const PrepConfig& config, std::string& error) {
    if (config.image_path.empty()) {
        error = "Falta la ruta de salida para exportar la imagen.";
        return false;
    }
    const ImageBuildResult result =
        runner_.build_image(config, config.image_path);
    if (!result.ok) {
        error = result.error;
        return false;
    }
    return true;
}

bool PrepController::export_default_image(PrepConfig& config,
                                          std::string& image_path,
                                          std::string& error,
                                          ExportGeometry geometry) {
    std::error_code code;
    const std::filesystem::path directory = "build";
    std::filesystem::create_directories(directory, code);
    if (code) {
        error = "No se pudo crear el directorio de exportación: " + code.message();
        return false;
    }

    // La exportación estándar no depende de ninguna USB: usa la geometría del
    // ejemplo de 64 MiB/512 que también emplean la CLI y la demo. La exportación
    // seleccionada conserva tamaño, sector y etiqueta del destino elegido.
    PrepConfig target = config;
    std::string name = "booteable-export";
    if (geometry == ExportGeometry::Standard) {
        target.device.path = "(estandar)";
        target.device.size_bytes = 64ull * kMib;
        target.device.logical_sector = 512;
        target.esp_bytes = 0;
        target.overwrite_confirmed = true;
        name += "-64mib";
    } else {
        const std::size_t slash = config.device.path.find_last_of('/');
        if (slash != std::string::npos &&
            slash + 1 < config.device.path.size()) {
            name += "-" + config.device.path.substr(slash + 1);
        }
    }

    image_path = (directory / (name + ".img")).string();
    target.image_path = image_path;
    if (!export_image(target, error)) {
        return false;
    }
    config.image_path = image_path;
    return true;
}

} // namespace booteable
