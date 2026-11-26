#include "app/FakePrepController.hpp"

#include "domain/Validation.hpp"
#include "TestHarness.hpp"

namespace {

using booteable::FakePrepController;
using booteable::PrepConfig;
using booteable::PrepOutcome;
using booteable::PrepStage;
using booteable::SafetyVerdict;

PrepConfig valid_config(const booteable::DeviceInfo& device) {
    PrepConfig config;
    config.device = device;
    config.overwrite_confirmed = true;
    return config;
}

} // namespace

void run_fake_controller_tests() {
    FakePrepController controller;
    const auto devices = controller.devices();
    CHECK_EQ(devices.size(), std::size_t{4});

    {
        const auto report = controller.safety(devices[0]);
        CHECK(report.verdict == SafetyVerdict::AllowedWithWarning);
    }
    {
        const auto report = controller.safety(devices[1]);
        CHECK(report.verdict == SafetyVerdict::Allowed);
    }
    {
        const auto report = controller.safety(devices[2]);
        CHECK(report.verdict == SafetyVerdict::Rejected);
        CHECK(!report.reasons.empty());
    }
    {
        const auto report = controller.safety(devices[3]);
        CHECK(report.verdict == SafetyVerdict::Rejected);
    }

    {
        const PrepConfig config = valid_config(devices[0]);
        CHECK(controller.validate(config).ok());
        CHECK(!controller.running());
        CHECK(controller.start(config));
        CHECK(!controller.running());
        CHECK_EQ(controller.progress().stage, PrepStage::Done);
        CHECK_EQ(controller.progress().percent, 100);
        CHECK(controller.result().outcome == PrepOutcome::Verified);
        CHECK(controller.result().verified);
        CHECK(controller.result().synchronized);
    }

    {
        PrepConfig config = valid_config(devices[0]);
        config.overwrite_confirmed = false;
        CHECK(!controller.start(config));
        CHECK(controller.result().outcome == PrepOutcome::InvalidTarget);
        CHECK_EQ(controller.result().failed_stage, PrepStage::Validating);
    }

    {
        FakePrepController failing;
        const PrepConfig config = valid_config(devices[0]);
        failing.fail_next(PrepOutcome::DeviceError, PrepStage::Writing,
                          "falló la escritura");
        CHECK(!failing.start(config));
        CHECK(failing.result().outcome == PrepOutcome::DeviceError);
        CHECK_EQ(failing.result().failed_stage, PrepStage::Writing);
        CHECK(!failing.running());
    }

    {
        // El fake no debe aceptar un 4Kn por debajo del mínimo de FAT32.
        FakePrepController four_kn;
        PrepConfig config = valid_config(devices[0]);
        config.device.logical_sector = 4096;
        config.esp_bytes = 64ull * 1024ull * 1024ull;
        CHECK(!four_kn.start(config));
        CHECK(four_kn.result().outcome == PrepOutcome::InvalidTarget);
    }
}
