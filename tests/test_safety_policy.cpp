#include <cstdint>
#include <string>

#include "TestHarness.hpp"
#include "device/SafetyPolicy.hpp"
#include "domain/Types.hpp"

namespace {

using booteable::DeviceInfo;
using booteable::DevicePartition;
using booteable::DeviceTransport;
using booteable::SafetyPolicy;
using booteable::SafetyReport;
using booteable::SafetyVerdict;
using booteable::SystemContext;

DeviceInfo make_usb(std::uint32_t sector = 512) {
    DeviceInfo device;
    device.path = "/dev/sdb";
    device.by_id = "/dev/disk/by-id/usb-Generic_Flash_Disk_1234";
    device.model = "Generic Flash Disk";
    device.serial = "1234ABCD";
    device.size_bytes = 8ull * 1024ull * 1024ull * 1024ull;
    device.logical_sector = sector;
    device.removable = true;
    device.transport = DeviceTransport::Usb;
    device.dev_id = "8:16";
    return device;
}

SystemContext make_context() {
    SystemContext context;
    context.root_device = "sda";
    context.boot_device = "sda";
    context.mounted_device_names = {"sda", "sdb"};
    return context;
}

bool has_reason(const SafetyReport& report, const std::string& needle) {
    for (const std::string& reason : report.reasons) {
        if (reason.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool has_warning(const SafetyReport& report, const std::string& needle) {
    for (const std::string& warning : report.warnings) {
        if (warning.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void test_allowed_usb() {
    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(make_usb(), make_context());
    CHECK(report.verdict == SafetyVerdict::Allowed);
    CHECK(report.reasons.empty());
    CHECK(report.warnings.empty());
}

void test_mounted_pendrive_rejected() {
    DeviceInfo device = make_usb();
    DevicePartition partition;
    partition.name = "sdb1";
    partition.mount_point = "/media/user/USB";
    partition.dev_id = "8:17";
    device.partitions.push_back(partition);

    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(device, make_context());
    CHECK(report.verdict == SafetyVerdict::Rejected);
    CHECK(has_reason(report, "montadas"));

    // Al escribir, MountManager se encarga del desmontaje y la política solo
    // advierte; no debe rechazar por estar montado.
    const SafetyReport write_report =
        policy.evaluate_for_write(device, make_context());
    CHECK(write_report.verdict == SafetyVerdict::AllowedWithWarning);
    CHECK(has_warning(write_report, "desmontarán"));
    CHECK(!has_reason(write_report, "montadas"));
}

void test_root_disk_rejected() {
    DeviceInfo device = make_usb();
    device.path = "/dev/sda";
    device.by_id = "/dev/disk/by-id/usb-System_Disk_9999";
    device.dev_id = "8:0";

    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(device, make_context());
    CHECK(report.verdict == SafetyVerdict::Rejected);
    CHECK(has_reason(report, "disco del sistema"));
    CHECK(!has_reason(report, "No es un dispositivo USB"));
}

void test_nvme_rejected() {
    DeviceInfo device = make_usb();
    device.path = "/dev/nvme0n1";
    device.transport = DeviceTransport::Nvme;
    device.removable = false;

    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(device, make_context());
    CHECK(report.verdict == SafetyVerdict::Rejected);
    CHECK(has_reason(report, "No es un dispositivo USB"));
    CHECK(!has_reason(report, "número de serie"));
}

void test_no_serial_rejected() {
    DeviceInfo device = make_usb();
    device.serial.clear();

    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(device, make_context());
    CHECK(report.verdict == SafetyVerdict::Rejected);
    CHECK(has_reason(report, "número de serie"));
    CHECK(!has_reason(report, "No es un dispositivo USB"));
}

void test_no_by_id_rejected() {
    DeviceInfo device = make_usb();
    device.by_id.clear();

    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(device, make_context());
    CHECK(report.verdict == SafetyVerdict::Rejected);
    CHECK(has_reason(report, "by-id"));
}

void test_4kn_allowed() {
    const SafetyPolicy policy;
    const SafetyReport report =
        policy.evaluate(make_usb(4096), make_context());
    CHECK(report.verdict == SafetyVerdict::Allowed);
    CHECK(report.reasons.empty());
}

void test_odd_sector_rejected() {
    const SafetyPolicy policy;
    const SafetyReport report =
        policy.evaluate(make_usb(1024), make_context());
    CHECK(report.verdict == SafetyVerdict::Rejected);
    CHECK(has_reason(report, "Sector lógico no soportado"));
}

void test_small_device_rejected() {
    DeviceInfo device = make_usb();
    device.size_bytes = 16ull * 1024ull * 1024ull;

    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(device, make_context());
    CHECK(report.verdict == SafetyVerdict::Rejected);
    CHECK(has_reason(report, "Capacidad insuficiente"));
}

void test_holders_rejected() {
    DeviceInfo device = make_usb();
    device.holders = {"dm-0"};

    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(device, make_context());
    CHECK(report.verdict == SafetyVerdict::Rejected);
    CHECK(has_reason(report, "holders"));
}

void test_read_only_rejected() {
    DeviceInfo device = make_usb();
    device.read_only = true;

    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(device, make_context());
    CHECK(report.verdict == SafetyVerdict::Rejected);
    CHECK(has_reason(report, "solo lectura"));
}

void test_existing_partitions_warn() {
    DeviceInfo device = make_usb();
    DevicePartition partition;
    partition.name = "sdb1";
    partition.dev_id = "8:17";
    device.partitions.push_back(partition);

    const SafetyPolicy policy;
    const SafetyReport report = policy.evaluate(device, make_context());
    CHECK(report.verdict == SafetyVerdict::AllowedWithWarning);
    CHECK(has_warning(report, "ya tiene particiones"));
}

void test_minimum_size_grows_with_sector() {
    const std::uint64_t min512 = SafetyPolicy::minimum_device_bytes(512);
    const std::uint64_t min4kn = SafetyPolicy::minimum_device_bytes(4096);
    CHECK(min4kn > min512);
    CHECK(min512 >= 48ull * booteable::kMib);
    CHECK(min4kn >= 260ull * booteable::kMib);
}

} // namespace

void run_safety_policy_tests() {
    test_allowed_usb();
    test_mounted_pendrive_rejected();
    test_root_disk_rejected();
    test_nvme_rejected();
    test_no_serial_rejected();
    test_no_by_id_rejected();
    test_4kn_allowed();
    test_odd_sector_rejected();
    test_small_device_rejected();
    test_holders_rejected();
    test_read_only_rejected();
    test_existing_partitions_warn();
    test_minimum_size_grows_with_sector();
}

