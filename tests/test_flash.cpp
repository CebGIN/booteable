#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "EmbeddedBootImage.hpp"
#include "TestHarness.hpp"
#include "device/DeviceFlasher.hpp"
#include "device/FaultyDeviceWriter.hpp"
#include "device/FileDeviceWriter.hpp"
#include "device/MountManager.hpp"
#include "domain/Types.hpp"
#include "image/BootImageBuilder.hpp"
#include "image/ImageVerifier.hpp"

namespace {

using booteable::DeviceInfo;
using booteable::DevicePartition;
using booteable::FileDeviceWriter;
using booteable::FlashCallbacks;
using booteable::ImageBuildResult;
using booteable::ImageManifest;
using booteable::MountManager;
using booteable::PrepOutcome;
using booteable::PrepResult;
using booteable::PrepStage;

std::filesystem::path temp_path(const std::string& name) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / name;
    std::error_code error;
    std::filesystem::remove(path, error);
    return path;
}

bool build_image(const std::filesystem::path& path, std::uint64_t disk_mib,
                 std::uint32_t sector, ImageBuildResult& out) {
    booteable::BootImageSpec spec;
    spec.disk_bytes = disk_mib * booteable::kMib;
    spec.logical_sector = sector;
    spec.volume_label = "BOOTEABLE";
    out = booteable::build_boot_image(path, spec, booteable::kBootEfi,
                                      booteable::kBootEfiSize);
    return out.ok;
}

DeviceInfo file_device(const std::filesystem::path& path,
                       const ImageManifest& manifest, std::uint32_t sector) {
    DeviceInfo device;
    device.path = path.string();
    device.size_bytes = manifest.disk_bytes;
    device.logical_sector = sector;
    device.removable = true;
    device.transport = booteable::DeviceTransport::Virtual;
    device.serial = "regular-file";
    device.by_id = path.string();
    device.dev_id = "8:16";
    return device;
}

std::filesystem::path empty_mountinfo(const std::string& name) {
    const std::filesystem::path path = temp_path(name);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << "";
    return path;
}

void test_flash_512_roundtrip() {
    const std::filesystem::path image = temp_path("booteable-test-flash-512.img");
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-512-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-512-mountinfo");

    ImageBuildResult built;
    CHECK(build_image(image, 80, 512, built));
    if (!built.ok) {
        return;
    }

    const DeviceInfo device = file_device(dest, built.manifest, 512);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);
    const PrepResult result = booteable::flash_image_to_device(
        device, image, built.manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::Verified);
    CHECK(result.verified);
    CHECK(result.synchronized);
    CHECK(result.failed_stage == PrepStage::Idle);
    CHECK_EQ(result.image_sha256, built.manifest.manifest_sha256);
    CHECK_EQ(result.device_sha256, built.manifest.manifest_sha256);
    CHECK_EQ(result.bytes_written, built.manifest.disk_bytes);
    CHECK_EQ(std::filesystem::file_size(dest), built.manifest.disk_bytes);

    ImageManifest read_back;
    std::string error;
    const booteable::ImageVerifier verifier;
    CHECK(verifier.verify_against(dest, built.manifest, read_back, error));

    std::filesystem::remove(image);
    std::filesystem::remove(dest);
    std::filesystem::remove(mountinfo);
}

void test_flash_4kn_roundtrip() {
    const std::filesystem::path image =
        temp_path("booteable-test-flash-4kn.img");
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-4kn-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-4kn-mountinfo");

    ImageBuildResult built;
    CHECK(build_image(image, 320, 4096, built));
    if (!built.ok) {
        return;
    }

    const DeviceInfo device = file_device(dest, built.manifest, 4096);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);
    const PrepResult result = booteable::flash_image_to_device(
        device, image, built.manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::Verified);
    CHECK(result.verified);
    CHECK_EQ(result.device_sha256, built.manifest.manifest_sha256);
    CHECK_EQ(std::filesystem::file_size(dest), built.manifest.disk_bytes);

    std::filesystem::remove(image);
    std::filesystem::remove(dest);
    std::filesystem::remove(mountinfo);
}

void test_flash_disconnected_mid_write() {
    const std::filesystem::path image =
        temp_path("booteable-test-flash-enodev.img");
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-enodev-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-enodev-mountinfo");

    ImageBuildResult built;
    CHECK(build_image(image, 80, 512, built));
    if (!built.ok) {
        return;
    }

    const DeviceInfo device = file_device(dest, built.manifest, 512);
    FileDeviceWriter inner(true, true);
    booteable::FaultyDeviceWriter writer(inner);
    writer.fail_after(2 * booteable::kMib, ENODEV);
    MountManager mounts(mountinfo);

    const PrepResult result = booteable::flash_image_to_device(
        device, image, built.manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::Disconnected);
    CHECK(!result.verified);
    CHECK(result.failed_stage == PrepStage::Writing);
    CHECK(result.bytes_written > 0);
    CHECK(result.bytes_written < built.manifest.disk_bytes);
    CHECK(result.message.find("incompleto") != std::string::npos);

    std::filesystem::remove(image);
    std::filesystem::remove(dest);
    std::filesystem::remove(mountinfo);
}

void test_flash_cancelled_mid_write_is_incomplete() {
    const std::filesystem::path image =
        temp_path("booteable-test-flash-cancel.img");
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-cancel-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-cancel-mountinfo");

    ImageBuildResult built;
    CHECK(build_image(image, 80, 512, built));
    if (!built.ok) {
        return;
    }

    const DeviceInfo device = file_device(dest, built.manifest, 512);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);

    std::atomic_bool cancel{false};
    FlashCallbacks callbacks;
    callbacks.cancel = &cancel;
    callbacks.progress = [&cancel](const booteable::PrepProgress& progress) {
        if (progress.stage == PrepStage::Writing && progress.bytes_done > 0) {
            cancel.store(true);
        }
    };

    const PrepResult result = booteable::flash_image_to_device(
        device, image, built.manifest, writer, mounts, callbacks);

    CHECK(result.outcome == PrepOutcome::Incomplete);
    CHECK(!result.verified);
    CHECK(result.bytes_written > 0);
    CHECK(result.message.find("incompleto") != std::string::npos);

    std::filesystem::remove(image);
    std::filesystem::remove(dest);
    std::filesystem::remove(mountinfo);
}

void test_flash_cancelled_before_start() {
    const std::filesystem::path image =
        temp_path("booteable-test-flash-cancel-start.img");
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-cancel-start-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-cancel-start-mountinfo");

    ImageBuildResult built;
    CHECK(build_image(image, 80, 512, built));
    if (!built.ok) {
        return;
    }

    const DeviceInfo device = file_device(dest, built.manifest, 512);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);

    std::atomic_bool cancel{true};
    FlashCallbacks callbacks;
    callbacks.cancel = &cancel;

    const PrepResult result = booteable::flash_image_to_device(
        device, image, built.manifest, writer, mounts, callbacks);

    CHECK(result.outcome == PrepOutcome::Cancelled);
    CHECK(!result.verified);
    CHECK_EQ(result.bytes_written, 0ull);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(image);
    std::filesystem::remove(mountinfo);
}

void test_flash_rejects_size_mismatch() {
    const std::filesystem::path image =
        temp_path("booteable-test-flash-size.img");
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-size-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-size-mountinfo");

    ImageBuildResult built;
    CHECK(build_image(image, 80, 512, built));
    if (!built.ok) {
        return;
    }

    DeviceInfo device = file_device(dest, built.manifest, 512);
    device.size_bytes -= 512;
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);

    const PrepResult result = booteable::flash_image_to_device(
        device, image, built.manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::InvalidTarget);
    CHECK(result.failed_stage == PrepStage::Validating);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(image);
    std::filesystem::remove(mountinfo);
}

void test_flash_rejects_sector_mismatch() {
    const std::filesystem::path image =
        temp_path("booteable-test-flash-sector.img");
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-sector-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-sector-mountinfo");

    ImageBuildResult built;
    CHECK(build_image(image, 80, 512, built));
    if (!built.ok) {
        return;
    }

    const DeviceInfo device = file_device(dest, built.manifest, 4096);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);

    const PrepResult result = booteable::flash_image_to_device(
        device, image, built.manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::InvalidTarget);
    CHECK(result.message.find("sectores de 512") != std::string::npos);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(image);
    std::filesystem::remove(mountinfo);
}

void test_flash_disconnected_after_unmount() {
    const std::filesystem::path image =
        temp_path("booteable-test-flash-gone.img");
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-gone-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-gone-mountinfo");

    ImageBuildResult built;
    CHECK(build_image(image, 80, 512, built));
    if (!built.ok) {
        return;
    }

    const DeviceInfo device = file_device(dest, built.manifest, 512);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);

    FlashCallbacks callbacks;
    callbacks.refresh = [](const std::string&) {
        return std::optional<DeviceInfo>{};
    };

    const PrepResult result = booteable::flash_image_to_device(
        device, image, built.manifest, writer, mounts, callbacks);

    CHECK(result.outcome == PrepOutcome::Disconnected);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(image);
    std::filesystem::remove(mountinfo);
}

void test_flash_unmount_failure_does_not_open() {
    const std::filesystem::path image =
        temp_path("booteable-test-flash-unmount.img");
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-unmount-target.img");
    const std::filesystem::path mountinfo =
        temp_path("booteable-test-flash-unmount-mountinfo");
    {
        std::ofstream file(mountinfo, std::ios::binary | std::ios::trunc);
        file << "2 1 8:16 / /mnt/usb rw - vfat /dev/sdb1 rw\n";
    }

    ImageBuildResult built;
    CHECK(build_image(image, 80, 512, built));
    if (!built.ok) {
        return;
    }

    DeviceInfo device = file_device(dest, built.manifest, 512);
    DevicePartition partition;
    partition.name = "sdb1";
    partition.dev_id = "8:17";
    partition.mount_point = "/mnt/usb";
    device.partitions.push_back(partition);

    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo, [](const std::string&) { return -1; });

    const PrepResult result = booteable::flash_image_to_device(
        device, image, built.manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::UnmountError);
    CHECK(result.failed_stage == PrepStage::Unmounting);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(image);
    std::filesystem::remove(mountinfo);
}

} // namespace

void run_flash_tests() {
    test_flash_512_roundtrip();
    test_flash_4kn_roundtrip();
    test_flash_disconnected_mid_write();
    test_flash_cancelled_mid_write_is_incomplete();
    test_flash_cancelled_before_start();
    test_flash_rejects_size_mismatch();
    test_flash_rejects_sector_mismatch();
    test_flash_disconnected_after_unmount();
    test_flash_unmount_failure_does_not_open();
}
