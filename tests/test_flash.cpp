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

void test_flash_512_roundtrip(const std::filesystem::path& image,
                              const ImageManifest& manifest) {
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-512-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-512-mountinfo");

    const DeviceInfo device = file_device(dest, manifest, 512);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);
    const PrepResult result = booteable::flash_image_to_device(
        device, image, manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::Verified);
    CHECK(result.verified);
    CHECK(result.synchronized);
    CHECK(result.failed_stage == PrepStage::Idle);
    CHECK_EQ(result.image_sha256, manifest.manifest_sha256);
    CHECK_EQ(result.device_sha256, manifest.manifest_sha256);
    CHECK_EQ(result.bytes_written, manifest.disk_bytes);
    CHECK_EQ(std::filesystem::file_size(dest), manifest.disk_bytes);

    ImageManifest read_back;
    std::string error;
    const booteable::ImageVerifier verifier;
    CHECK(verifier.verify_against(dest, manifest, read_back, error));

    std::filesystem::remove(dest);
    std::filesystem::remove(mountinfo);
}

void test_flash_4kn_roundtrip(const std::filesystem::path& image,
                              const ImageManifest& manifest) {
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-4kn-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-4kn-mountinfo");

    const DeviceInfo device = file_device(dest, manifest, 4096);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);
    const PrepResult result = booteable::flash_image_to_device(
        device, image, manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::Verified);
    CHECK(result.verified);
    CHECK_EQ(result.device_sha256, manifest.manifest_sha256);
    CHECK_EQ(std::filesystem::file_size(dest), manifest.disk_bytes);

    std::filesystem::remove(dest);
    std::filesystem::remove(mountinfo);
}

void test_flash_disconnected_mid_write(const std::filesystem::path& image,
                                       const ImageManifest& manifest) {
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-enodev-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-enodev-mountinfo");

    const DeviceInfo device = file_device(dest, manifest, 512);
    FileDeviceWriter inner(true, true);
    booteable::FaultyDeviceWriter writer(inner);
    writer.fail_after(2 * booteable::kMib, ENODEV);
    MountManager mounts(mountinfo);

    const PrepResult result = booteable::flash_image_to_device(
        device, image, manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::Disconnected);
    CHECK(!result.verified);
    CHECK(result.failed_stage == PrepStage::Writing);
    CHECK(result.bytes_written > 0);
    CHECK(result.bytes_written < manifest.disk_bytes);
    CHECK(result.message.find("incompleto") != std::string::npos);

    std::filesystem::remove(dest);
    std::filesystem::remove(mountinfo);
}

void test_flash_cancelled_mid_write_is_incomplete(
    const std::filesystem::path& image, const ImageManifest& manifest) {
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-cancel-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-cancel-mountinfo");

    const DeviceInfo device = file_device(dest, manifest, 512);
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
        device, image, manifest, writer, mounts, callbacks);

    CHECK(result.outcome == PrepOutcome::Incomplete);
    CHECK(!result.verified);
    CHECK(result.bytes_written > 0);
    CHECK(result.message.find("incompleto") != std::string::npos);

    std::filesystem::remove(dest);
    std::filesystem::remove(mountinfo);
}

void test_flash_cancelled_before_start(const std::filesystem::path& image,
                                       const ImageManifest& manifest) {
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-cancel-start-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-cancel-start-mountinfo");

    const DeviceInfo device = file_device(dest, manifest, 512);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);

    std::atomic_bool cancel{true};
    FlashCallbacks callbacks;
    callbacks.cancel = &cancel;

    const PrepResult result = booteable::flash_image_to_device(
        device, image, manifest, writer, mounts, callbacks);

    CHECK(result.outcome == PrepOutcome::Cancelled);
    CHECK(!result.verified);
    CHECK_EQ(result.bytes_written, 0ull);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(mountinfo);
}

void test_flash_rejects_size_mismatch(const std::filesystem::path& image,
                                      const ImageManifest& manifest) {
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-size-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-size-mountinfo");

    DeviceInfo device = file_device(dest, manifest, 512);
    device.size_bytes -= 512;
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);

    const PrepResult result = booteable::flash_image_to_device(
        device, image, manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::InvalidTarget);
    CHECK(result.failed_stage == PrepStage::Validating);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(mountinfo);
}

void test_flash_rejects_sector_mismatch(const std::filesystem::path& image,
                                        const ImageManifest& manifest) {
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-sector-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-sector-mountinfo");

    const DeviceInfo device = file_device(dest, manifest, 4096);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);

    const PrepResult result = booteable::flash_image_to_device(
        device, image, manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::InvalidTarget);
    CHECK(result.message.find("sectores de 512") != std::string::npos);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(mountinfo);
}

void test_flash_disconnected_after_unmount(const std::filesystem::path& image,
                                           const ImageManifest& manifest) {
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-gone-target.img");
    const std::filesystem::path mountinfo =
        empty_mountinfo("booteable-test-flash-gone-mountinfo");

    const DeviceInfo device = file_device(dest, manifest, 512);
    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo);

    FlashCallbacks callbacks;
    callbacks.refresh = [](const std::string&) {
        return std::optional<DeviceInfo>{};
    };

    const PrepResult result = booteable::flash_image_to_device(
        device, image, manifest, writer, mounts, callbacks);

    CHECK(result.outcome == PrepOutcome::Disconnected);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(mountinfo);
}

void test_flash_unmount_failure_does_not_open(
    const std::filesystem::path& image, const ImageManifest& manifest) {
    const std::filesystem::path dest =
        temp_path("booteable-test-flash-unmount-target.img");
    const std::filesystem::path mountinfo =
        temp_path("booteable-test-flash-unmount-mountinfo");
    {
        std::ofstream file(mountinfo, std::ios::binary | std::ios::trunc);
        file << "2 1 8:16 / /mnt/usb rw - vfat /dev/sdb1 rw\n";
    }

    DeviceInfo device = file_device(dest, manifest, 512);
    DevicePartition partition;
    partition.name = "sdb1";
    partition.dev_id = "8:17";
    partition.mount_point = "/mnt/usb";
    device.partitions.push_back(partition);

    FileDeviceWriter writer(true, true);
    MountManager mounts(mountinfo, [](const std::string&) { return -1; });

    const PrepResult result = booteable::flash_image_to_device(
        device, image, manifest, writer, mounts);

    CHECK(result.outcome == PrepOutcome::UnmountError);
    CHECK(result.failed_stage == PrepStage::Unmounting);
    CHECK(!std::filesystem::exists(dest));

    std::filesystem::remove(mountinfo);
}

} // namespace

void run_flash_tests() {
    // Una sola imagen por geometría: las pruebas la leen y escriben destinos
    // distintos. Así la suite no reconstruye la misma imagen siete veces.
    const std::filesystem::path image512 =
        temp_path("booteable-test-flash-512.img");
    const std::filesystem::path image4kn =
        temp_path("booteable-test-flash-4kn.img");

    ImageBuildResult built512;
    ImageBuildResult built4kn;
    // 80 MiB es el caso de aceptación de WP3b; el resto de las pruebas reutiliza
    // la misma imagen en vez de reconstruirla.
    CHECK(build_image(image512, 80, 512, built512));
    CHECK(build_image(image4kn, 320, 4096, built4kn));

    if (built512.ok && built4kn.ok) {
        test_flash_512_roundtrip(image512, built512.manifest);
        test_flash_4kn_roundtrip(image4kn, built4kn.manifest);
        test_flash_disconnected_mid_write(image512, built512.manifest);
        test_flash_cancelled_mid_write_is_incomplete(image512,
                                                     built512.manifest);
        test_flash_cancelled_before_start(image512, built512.manifest);
        test_flash_rejects_size_mismatch(image512, built512.manifest);
        test_flash_rejects_sector_mismatch(image512, built512.manifest);
        test_flash_disconnected_after_unmount(image512, built512.manifest);
        test_flash_unmount_failure_does_not_open(image512,
                                                 built512.manifest);
    }

    std::error_code ignored;
    std::filesystem::remove(image512, ignored);
    std::filesystem::remove(image4kn, ignored);
}
