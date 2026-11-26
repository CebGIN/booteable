#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "app/PrepController.hpp"
#include "app/PrepRunner.hpp"
#include "ui/AppScene.hpp"
#include "TestHarness.hpp"

namespace {

using booteable::DeviceInfo;
using booteable::DeviceTransport;
using booteable::IPrepRunner;
using booteable::ImageBuildResult;
using booteable::PrepConfig;
using booteable::PrepController;
using booteable::PrepOutcome;
using booteable::PrepProgress;
using booteable::PrepResult;
using booteable::PrepStage;
using booteable::SystemContext;
using booteable::ui::AppScene;
using booteable::ui::UiScreen;

class FakeSource : public booteable::IBlockDeviceSource {
public:
    explicit FakeSource(std::string path = "/dev/sdb")
        : device_() {
        device_.path = std::move(path);
        device_.by_id = "/dev/disk/by-id/usb-Fake_128MB_0001";
        device_.model = "Fake USB 128MB";
        device_.serial = "FAKE128";
        device_.size_bytes = 128ull * 1024ull * 1024ull;
        device_.logical_sector = 512;
        device_.removable = true;
        device_.transport = DeviceTransport::Usb;
    }

    std::vector<DeviceInfo> list() override { return {device_}; }
    std::optional<DeviceInfo> refresh(const std::string& path) override {
        if (path == device_.path) {
            return device_;
        }
        return std::nullopt;
    }
    SystemContext system_context() override { return SystemContext{}; }

    const DeviceInfo& device() const { return device_; }

private:
    DeviceInfo device_;
};

class FakeRunner : public IPrepRunner {
public:
    std::atomic<int> runs{0};
    std::atomic<int> builds{0};
    bool wait_for_cancel = false;
    bool throw_now = false;
    bool build_ok = true;

    PrepResult run(const PrepConfig& config, const std::atomic_bool& cancel,
                   const std::function<void(const PrepProgress&)>& progress) override {
        ++runs;
        if (throw_now) {
            throw std::runtime_error("fallo simulado del runner");
        }

        if (progress) {
            PrepProgress value;
            value.stage = PrepStage::Writing;
            value.percent = 50;
            value.bytes_total = config.device.size_bytes;
            value.bytes_done = config.device.size_bytes / 2;
            value.detail = "Escribiendo la imagen (simulado)";
            progress(value);
        }

        if (wait_for_cancel) {
            while (!cancel.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            PrepResult result;
            result.outcome = PrepOutcome::Incomplete;
            result.failed_stage = PrepStage::Writing;
            result.message = "Cancelado durante la escritura.";
            result.bytes_written = config.device.size_bytes / 2;
            return result;
        }

        PrepResult result;
        result.outcome = PrepOutcome::Verified;
        result.message = "Verificado (simulado).";
        result.image_sha256 = "image-hash";
        result.device_sha256 = "image-hash";
        result.bytes_written = config.device.size_bytes;
        result.synchronized = true;
        result.verified = true;
        return result;
    }

    ImageBuildResult build_image(const PrepConfig& config,
                                 const std::filesystem::path&) override {
        ++builds;
        ImageBuildResult result;
        if (!build_ok) {
            result.error = "construcción simulada fallida";
            return result;
        }
        result.ok = true;
        result.esp_bytes = config.esp_bytes;
        result.layout.sector_size = config.device.logical_sector;
        result.manifest.disk_bytes = config.device.size_bytes;
        result.manifest.manifest_sha256 = "manifest-hash";
        return result;
    }
};

PrepConfig valid_config() {
    PrepConfig config;
    config.device.path = "/dev/sdb";
    config.device.by_id = "/dev/disk/by-id/usb-Fake_128MB_0001";
    config.device.model = "Fake USB 128MB";
    config.device.serial = "FAKE128";
    config.device.size_bytes = 128ull * 1024ull * 1024ull;
    config.device.logical_sector = 512;
    config.device.removable = true;
    config.device.transport = DeviceTransport::Usb;
    config.overwrite_confirmed = true;
    return config;
}

template <typename Predicate>
bool wait_for(Predicate predicate, int milliseconds = 3000) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}

void test_controller_validation() {
    FakeSource source;
    FakeRunner runner;
    PrepController controller(source, runner, {}, [] { return true; });

    PrepConfig config = valid_config();
    config.overwrite_confirmed = false;
    CHECK(!controller.validate(config).ok());
    CHECK(!controller.start(config));
    CHECK_EQ(runner.runs.load(), 0);
    CHECK(controller.result().outcome == PrepOutcome::InvalidTarget);
    CHECK_EQ(controller.result().failed_stage, PrepStage::Validating);
}

void test_permission_denied() {
    FakeSource source;
    FakeRunner runner;
    PrepController controller(source, runner, {}, [] { return false; });

    CHECK(!controller.can_write());
    CHECK(controller.write_block_reason().find("sudo") != std::string::npos);
    CHECK(!controller.start(valid_config()));
    CHECK_EQ(runner.runs.load(), 0);
    CHECK(controller.result().outcome == PrepOutcome::PermissionDenied);
    CHECK(controller.result().message.find("sudo") != std::string::npos);
}

void test_export_image() {
    FakeSource source;
    FakeRunner runner;
    PrepController controller(source, runner, {}, [] { return true; });

    PrepConfig config = valid_config();
    config.image_path = "build/test-export.img";
    std::string error;
    CHECK(controller.export_image(config, error));
    CHECK_EQ(runner.builds.load(), 1);
    CHECK(error.empty());

    runner.build_ok = false;
    CHECK(!controller.export_image(config, error));
    CHECK(error.find("simulada") != std::string::npos);
}

void test_cancel_during_write() {
    FakeSource source;
    FakeRunner runner;
    runner.wait_for_cancel = true;
    PrepController controller(source, runner, {}, [] { return true; });

    CHECK(controller.start(valid_config()));
    CHECK(controller.running());
    CHECK(wait_for([&controller] {
        return controller.progress().stage == PrepStage::Writing;
    }));
    controller.cancel();
    CHECK(wait_for([&controller] { return !controller.running(); }));
    CHECK(controller.result().outcome == PrepOutcome::Incomplete);
    CHECK_EQ(controller.progress().stage, PrepStage::Incomplete);
}

void test_runner_exception_does_not_crash() {
    FakeSource source;
    FakeRunner runner;
    runner.throw_now = true;
    PrepController controller(source, runner, {}, [] { return true; });

    CHECK(controller.start(valid_config()));
    CHECK(wait_for([&controller] { return !controller.running(); }));
    CHECK(controller.result().outcome == PrepOutcome::DeviceError);
    CHECK(controller.result().message.find("simulado") != std::string::npos);
}

void test_ui_with_real_controller() {
    FakeSource source;
    FakeRunner runner;
    PrepController controller(source, runner, {}, [] { return true; });
    AppScene app(controller);

    auto root = app.build();
    root->update_layout(guayaba::Rect::from_size(guayaba::Vec2{80, 24}));

    app.handle_key(guayaba::Key::ENTER);
    CHECK(app.state().screen == UiScreen::Confirm);
    app.set_confirmed(true);
    app.handle_key(guayaba::Key::ENTER);
    CHECK(app.state().screen == UiScreen::Progress);

    CHECK(wait_for([&app] {
        app.tick();
        return app.state().screen == UiScreen::Result;
    }));
    CHECK(app.state().result.outcome == PrepOutcome::Verified);
    CHECK_EQ(app.progress_bar()->get_value(), 100.0);
    CHECK_EQ(app.result_outcome()->get_text(), std::string("VERIFICADO"));
}

void test_export_without_privileges_from_ui() {
    FakeSource source;
    FakeRunner runner;
    PrepController controller(source, runner, {}, [] { return false; });
    AppScene app(controller);

    auto root = app.build();
    root->update_layout(guayaba::Rect::from_size(guayaba::Vec2{80, 24}));

    CHECK(!app.state().can_write);
    CHECK(app.prepare_button()->is_disabled());
    app.handle_key(guayaba::Key::F8);
    CHECK_EQ(runner.builds.load(), 1);
    CHECK(app.state().status.find("exportada") != std::string::npos);
}

void test_destructor_joins_running_worker() {
    FakeSource source;
    FakeRunner runner;
    runner.wait_for_cancel = true;
    {
        PrepController controller(source, runner, {}, [] { return true; });
        CHECK(controller.start(valid_config()));
        CHECK(wait_for([&controller] {
            return controller.progress().stage == PrepStage::Writing;
        }));
    }
    CHECK(true);
}

void test_real_runner_to_regular_file() {
    const std::filesystem::path target =
        std::filesystem::temp_directory_path() / "booteable-wp4b-target.img";
    std::error_code ignored;
    std::filesystem::remove(target, ignored);
    {
        std::ofstream create(target, std::ios::binary | std::ios::trunc);
        CHECK(create.good());
    }
    std::filesystem::resize_file(target, 128ull * 1024ull * 1024ull);

    FakeSource source(target.string());
    booteable::PrepRunner runner(source);

    PrepConfig config = valid_config();
    config.device = source.device();
    config.image_path.clear();

    std::atomic_bool cancel{false};
    int progress_events = 0;
    const PrepResult result = runner.run(
        config, cancel, [&progress_events](const PrepProgress&) {
            ++progress_events;
        });

    CHECK(result.outcome == PrepOutcome::Verified);
    CHECK(result.verified);
    CHECK(result.synchronized);
    CHECK(progress_events > 0);
    CHECK_EQ(std::filesystem::file_size(target),
              std::uintmax_t{128ull * 1024ull * 1024ull});
    CHECK(!result.image_sha256.empty());
    CHECK_EQ(result.image_sha256, result.device_sha256);

    std::filesystem::remove(target, ignored);
}

} // namespace

void run_prep_controller_tests() {
    test_controller_validation();
    test_permission_denied();
    test_export_image();
    test_cancel_during_write();
    test_runner_exception_does_not_crash();
    test_ui_with_real_controller();
    test_export_without_privileges_from_ui();
    test_destructor_joins_running_worker();
    test_real_runner_to_regular_file();
}
