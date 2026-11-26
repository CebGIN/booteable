#include "domain/Validation.hpp"

#include "TestHarness.hpp"

namespace {

using booteable::PrepConfig;
using booteable::ValidationReport;

PrepConfig make_config(std::uint32_t logical_sector = 512,
                       std::uint64_t size_bytes = 16ull * 1024ull * 1024ull * 1024ull) {
    PrepConfig config;
    config.device.path = "/dev/sdb";
    config.device.by_id = "/dev/disk/by-id/usb-Test_0001";
    config.device.model = "Test USB";
    config.device.serial = "0001";
    config.device.logical_sector = logical_sector;
    config.device.size_bytes = size_bytes;
    config.device.removable = true;
    config.device.transport = booteable::DeviceTransport::Usb;
    config.volume_label = "BOOTEABLE";
    config.overwrite_confirmed = true;
    return config;
}

bool has_issue(const ValidationReport& report, const std::string& field) {
    for (const auto& issue : report.issues) {
        if (issue.field == field) {
            return true;
        }
    }
    return false;
}

} // namespace

void run_validation_tests() {
    {
        const PrepConfig config = make_config();
        const ValidationReport report = booteable::validate_config(config);
        CHECK(report.ok());
        CHECK_EQ(booteable::effective_esp_bytes(config), 64ull * 1024ull * 1024ull);
    }

    {
        const PrepConfig config = make_config(4096);
        const ValidationReport report = booteable::validate_config(config);
        CHECK(report.ok());
        CHECK_EQ(booteable::effective_esp_bytes(config), 260ull * 1024ull * 1024ull);
    }

    {
        PrepConfig config = make_config(4096);
        config.esp_bytes = 64ull * 1024ull * 1024ull;
        const ValidationReport report = booteable::validate_config(config);
        CHECK(!report.ok());
        CHECK(has_issue(report, "esp_bytes"));
    }

    {
        PrepConfig config = make_config();
        config.esp_bytes = 48ull * 1024ull * 1024ull;
        CHECK(booteable::validate_config(config).ok());
    }

    {
        PrepConfig config = make_config();
        config.esp_bytes = 47ull * 1024ull * 1024ull;
        const ValidationReport report = booteable::validate_config(config);
        CHECK(!report.ok());
        CHECK(has_issue(report, "esp_bytes"));
    }

    {
        PrepConfig config = make_config();
        config.device.logical_sector = 1024;
        const ValidationReport report = booteable::validate_config(config);
        CHECK(!report.ok());
        CHECK(has_issue(report, "logical_sector"));
    }

    {
        PrepConfig config = make_config();
        config.device.size_bytes = 16ull * 1024ull * 1024ull * 1024ull + 1;
        const ValidationReport report = booteable::validate_config(config);
        CHECK(!report.ok());
        CHECK(has_issue(report, "device.size_bytes"));
    }

    {
        const PrepConfig config = make_config(512, 8ull * 1024ull * 1024ull);
        const ValidationReport report = booteable::validate_config(config);
        CHECK(!report.ok());
        CHECK(has_issue(report, "esp_bytes"));
    }

    {
        PrepConfig config = make_config();
        config.volume_label = "booteable";
        CHECK(!booteable::validate_config(config).ok());
    }

    {
        PrepConfig config = make_config();
        config.volume_label = "";
        CHECK(!booteable::validate_config(config).ok());
    }

    {
        PrepConfig config = make_config();
        config.volume_label = "DOCE-CARACTE";
        CHECK(!booteable::validate_config(config).ok());
    }

    {
        PrepConfig config = make_config();
        config.volume_label = "BOOT EABLE";
        CHECK(booteable::validate_config(config).ok());
    }

    {
        PrepConfig config = make_config();
        config.volume_label = " BOOTEABLE";
        CHECK(!booteable::validate_config(config).ok());
    }

    {
        PrepConfig config = make_config();
        config.overwrite_confirmed = false;
        const ValidationReport report = booteable::validate_config(config);
        CHECK(!report.ok());
        CHECK(has_issue(report, "overwrite_confirmed"));
    }

    {
        const PrepConfig config = make_config();
        CHECK(!booteable::validate_efi_size(config, 0).ok());
        CHECK(booteable::validate_efi_size(config, 64ull * 1024ull).ok());
        CHECK(!booteable::validate_efi_size(config, 64ull * 1024ull * 1024ull).ok());
        CHECK(booteable::validate(config, 64ull * 1024ull).ok());
    }
}
