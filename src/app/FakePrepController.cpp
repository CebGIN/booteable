#include "app/FakePrepController.hpp"

#include <utility>

#include "domain/Validation.hpp"

namespace booteable {
namespace {

DeviceInfo make_device(std::string path, std::string by_id, std::string model,
                       std::string serial, std::uint64_t size_bytes,
                       DeviceTransport transport, bool removable,
                       std::uint32_t logical_sector = 512) {
    DeviceInfo device;
    device.path = std::move(path);
    device.by_id = std::move(by_id);
    device.model = std::move(model);
    device.serial = std::move(serial);
    device.size_bytes = size_bytes;
    device.logical_sector = logical_sector;
    device.transport = transport;
    device.removable = removable;
    return device;
}

} // namespace

FakePrepController::FakePrepController(std::vector<DeviceInfo> devices)
    : devices_(std::move(devices)) {}

std::vector<DeviceInfo> FakePrepController::default_devices() {
    DeviceInfo usb = make_device(
        "/dev/sdb", "/dev/disk/by-id/usb-Fake_USB_16GB_0001",
        "Fake USB 3.0", "FAKE0001", 16ull * 1024ull * 1024ull * 1024ull,
        DeviceTransport::Usb, true);
    usb.partitions.push_back(DevicePartition{
        "sdb1", "/media/usb", 8ull * 1024ull * 1024ull * 1024ull, "8:17"});

    DeviceInfo small = make_device(
        "/dev/sdc", "/dev/disk/by-id/usb-Fake_USB_2GB_0002",
        "Fake USB 2.0", "FAKE0002", 2ull * 1024ull * 1024ull * 1024ull,
        DeviceTransport::Usb, true);

    DeviceInfo internal = make_device(
        "/dev/nvme0n1", "/dev/disk/by-id/nvme-Fake_SSD_0003",
        "Fake SSD", "FAKE0003", 512ull * 1024ull * 1024ull * 1024ull,
        DeviceTransport::Nvme, false);

    DeviceInfo no_serial = make_device(
        "/dev/sdd", "", "Fake USB sin serie", "",
        8ull * 1024ull * 1024ull * 1024ull, DeviceTransport::Usb, true);

    return {usb, small, internal, no_serial};
}

std::vector<DeviceInfo> FakePrepController::devices() {
    return devices_;
}

SafetyReport FakePrepController::safety(const DeviceInfo& device) {
    SafetyReport report;
    report.verdict = SafetyVerdict::Allowed;

    if (device.transport != DeviceTransport::Usb) {
        report.reasons.push_back("No es un dispositivo USB.");
    }
    if (!device.removable) {
        report.reasons.push_back("El dispositivo no es extraíble.");
    }
    if (device.serial.empty() || device.by_id.empty()) {
        report.reasons.push_back("La identidad del dispositivo no es comprobable.");
    }
    if (device.read_only) {
        report.reasons.push_back("El dispositivo está en solo lectura.");
    }
    if (!is_supported_sector(device.logical_sector)) {
        report.reasons.push_back("El sector lógico no está soportado.");
    }
    if (device.size_bytes < kMinEspBytes512) {
        report.reasons.push_back("La capacidad es demasiado pequeña.");
    }
    for (const DevicePartition& partition : device.partitions) {
        if (!partition.mount_point.empty()) {
            report.warnings.push_back("Hay particiones montadas que se desmontarán.");
            break;
        }
    }

    if (!report.reasons.empty()) {
        report.verdict = SafetyVerdict::Rejected;
    } else if (!report.warnings.empty()) {
        report.verdict = SafetyVerdict::AllowedWithWarning;
    }
    return report;
}

ValidationReport FakePrepController::validate(const PrepConfig& config) {
    return validate_config(config);
}

bool FakePrepController::start(const PrepConfig& config) {
    if (running_) {
        return false;
    }

    const ValidationReport report = validate(config);
    if (!report.ok()) {
        progress_.stage = PrepStage::Failed;
        progress_.detail = report.issues.front().message;
        result_ = PrepResult{};
        result_.outcome = PrepOutcome::InvalidTarget;
        result_.failed_stage = PrepStage::Validating;
        result_.message = report.issues.front().message;
        return false;
    }

    running_ = true;

    auto advance = [this](PrepStage stage, int percent, std::string detail) {
        progress_.stage = stage;
        progress_.percent = percent;
        progress_.detail = std::move(detail);
    };

    auto fail_here = [this](PrepStage stage) {
        if (!fail_armed_ || fail_stage_ != stage) {
            return false;
        }
        running_ = false;
        fail_armed_ = false;
        progress_.stage = PrepStage::Failed;
        result_ = PrepResult{};
        result_.outcome = fail_outcome_;
        result_.failed_stage = stage;
        result_.message = fail_message_;
        return true;
    };

    advance(PrepStage::Building, 10, "Construyendo la imagen");
    if (fail_here(PrepStage::Building)) {
        return false;
    }
    advance(PrepStage::Unmounting, 20, "Desmontando particiones");
    if (fail_here(PrepStage::Unmounting)) {
        return false;
    }
    advance(PrepStage::Opening, 30, "Abriendo el dispositivo");
    if (fail_here(PrepStage::Opening)) {
        return false;
    }
    advance(PrepStage::Writing, 60, "Escribiendo la imagen");
    if (fail_here(PrepStage::Writing)) {
        return false;
    }
    advance(PrepStage::Syncing, 80, "Sincronizando");
    if (fail_here(PrepStage::Syncing)) {
        return false;
    }
    advance(PrepStage::Verifying, 95, "Verificando la imagen");
    if (fail_here(PrepStage::Verifying)) {
        return false;
    }

    running_ = false;
    progress_.bytes_done = config.device.size_bytes;
    progress_.bytes_total = config.device.size_bytes;
    advance(PrepStage::Done, 100, "Imagen verificada");

    result_ = PrepResult{};
    result_.outcome = PrepOutcome::Verified;
    result_.failed_stage = PrepStage::Idle;
    result_.message = "La imagen se construyó, escribió y verificó (simulado).";
    result_.image_sha256 = "fake-image-sha256";
    result_.device_sha256 = "fake-image-sha256";
    result_.bytes_written = config.device.size_bytes;
    result_.synchronized = true;
    result_.verified = true;
    return true;
}

void FakePrepController::cancel() {
    if (!running_) {
        return;
    }
    const PrepStage interrupted = progress_.stage;
    running_ = false;
    progress_.stage = PrepStage::Cancelled;
    progress_.detail = "Cancelado por el usuario";
    result_ = PrepResult{};
    result_.outcome = PrepOutcome::Cancelled;
    result_.failed_stage = interrupted;
    result_.message = "La preparación se canceló.";
}

PrepProgress FakePrepController::progress() const {
    return progress_;
}

PrepResult FakePrepController::result() const {
    return result_;
}

bool FakePrepController::running() const {
    return running_;
}

void FakePrepController::fail_next(PrepOutcome outcome, PrepStage stage,
                                   std::string message) {
    fail_armed_ = true;
    fail_outcome_ = outcome;
    fail_stage_ = stage;
    fail_message_ = std::move(message);
}

} // namespace booteable
