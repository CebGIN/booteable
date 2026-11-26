#include <cerrno>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "TestHarness.hpp"
#include "device/DeviceIdentity.hpp"
#include "device/MountManager.hpp"
#include "domain/Types.hpp"

namespace {

using booteable::DeviceInfo;
using booteable::DevicePartition;
using booteable::MountEntry;
using booteable::MountManager;

std::filesystem::path temp_mountinfo(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

DeviceInfo make_device() {
    DeviceInfo device;
    device.path = "/dev/sdb";
    device.dev_id = "8:16";
    device.serial = "1234";
    device.by_id = "/dev/disk/by-id/usb-1234";

    DevicePartition first;
    first.name = "sdb1";
    first.dev_id = "8:17";
    first.mount_point = "/mnt/usb";
    device.partitions.push_back(first);

    DevicePartition second;
    second.name = "sdb2";
    second.dev_id = "8:18";
    second.mount_point = "/mnt/usb/nested";
    device.partitions.push_back(second);
    return device;
}

// Quita del archivo las líneas cuyo punto de montaje coincide.
void remove_mount_line(const std::filesystem::path& path,
                       const std::string& mount_point) {
    std::ifstream input(path);
    std::vector<std::string> kept;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        std::vector<std::string> fields;
        std::string field;
        while (stream >> field) {
            fields.push_back(field);
        }
        if (fields.size() > 4 && fields[4] == mount_point) {
            continue;
        }
        kept.push_back(line);
    }
    input.close();

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    for (const std::string& remaining : kept) {
        output << remaining << '\n';
    }
}

void test_unmounts_only_target_deepest_first() {
    const std::filesystem::path info =
        temp_mountinfo("booteable-test-mountinfo-target");
    write_text(info,
               "1 0 8:1 / / rw - ext4 /dev/sda1 rw\n"
               "2 1 8:17 / /mnt/usb rw - vfat /dev/sdb1 rw\n"
               "3 2 8:18 / /mnt/usb/nested rw - vfat /dev/sdb2 rw\n");

    std::vector<std::string> calls;
    MountManager manager(info, [&info, &calls](const std::string& mount_point) {
        calls.push_back(mount_point);
        remove_mount_line(info, mount_point);
        return 0;
    });

    const DeviceInfo device = make_device();
    CHECK(manager.has_mounts(device));
    CHECK_EQ(manager.mounts_for(device).size(), std::size_t{2});

    std::vector<std::string> unmounted;
    std::string error;
    CHECK(manager.unmount_all(device, unmounted, error));
    CHECK(error.empty());
    CHECK_EQ(calls.size(), std::size_t{2});
    CHECK_EQ(unmounted.size(), std::size_t{2});
    if (calls.size() == 2) {
        CHECK_EQ(calls[0], std::string("/mnt/usb/nested"));
        CHECK_EQ(calls[1], std::string("/mnt/usb"));
    }
    if (unmounted.size() == 2) {
        CHECK_EQ(unmounted[0], std::string("/mnt/usb/nested"));
        CHECK_EQ(unmounted[1], std::string("/mnt/usb"));
    }

    // El montaje de la raíz no se tocó.
    const std::vector<MountEntry> remaining = booteable::read_mountinfo(info);
    CHECK_EQ(remaining.size(), std::size_t{1});
    if (!remaining.empty()) {
        CHECK_EQ(remaining[0].mount_point, std::string("/"));
    }
    CHECK(!manager.has_mounts(device));

    std::filesystem::remove(info);
}

void test_unmount_failure_stops() {
    const std::filesystem::path info =
        temp_mountinfo("booteable-test-mountinfo-failure");
    write_text(info,
               "2 1 8:17 / /mnt/usb rw - vfat /dev/sdb1 rw\n"
               "3 2 8:18 / /mnt/usb/nested rw - vfat /dev/sdb2 rw\n");

    std::vector<std::string> calls;
    MountManager manager(info, [&calls](const std::string& mount_point) {
        calls.push_back(mount_point);
        errno = EBUSY;
        return -1;
    });

    std::vector<std::string> unmounted;
    std::string error;
    CHECK(!manager.unmount_all(make_device(), unmounted, error));
    CHECK(!error.empty());
    CHECK_EQ(calls.size(), std::size_t{1});
    if (!calls.empty()) {
        CHECK_EQ(calls[0], std::string("/mnt/usb/nested"));
    }

    std::filesystem::remove(info);
}

void test_unmount_verified_after() {
    const std::filesystem::path info =
        temp_mountinfo("booteable-test-mountinfo-unverified");
    write_text(info, "2 1 8:17 / /mnt/usb rw - vfat /dev/sdb1 rw\n");

    // El desmontaje "tiene éxito" pero el montaje sigue visible: la relectura
    // debe detectarlo y no declarar el destino listo.
    MountManager manager(info, [](const std::string&) { return 0; });

    std::vector<std::string> unmounted;
    std::string error;
    CHECK(!manager.unmount_all(make_device(), unmounted, error));
    CHECK(error.find("Quedan montajes") != std::string::npos);

    std::filesystem::remove(info);
}

void test_no_mounts_is_success() {
    const std::filesystem::path info =
        temp_mountinfo("booteable-test-mountinfo-none");
    write_text(info, "1 0 8:1 / / rw - ext4 /dev/sda1 rw\n");

    bool called = false;
    MountManager manager(info, [&called](const std::string&) {
        called = true;
        return 0;
    });

    std::vector<std::string> unmounted;
    std::string error;
    CHECK(manager.unmount_all(make_device(), unmounted, error));
    CHECK(error.empty());
    CHECK(!called);
    CHECK(unmounted.empty());

    std::filesystem::remove(info);
}

void test_mounts_by_mount_point_fallback() {
    // Sin dev_id, el punto de montaje de la partición sigue identificando el
    // montaje. Es el caso de los fixtures de la política.
    const std::vector<MountEntry> entries = {
        {"/mnt/usb", "", "vfat", "/dev/sdb1"},
        {"/", "", "ext4", "/dev/sda1"}};

    DeviceInfo device;
    device.path = "/dev/sdb";
    DevicePartition partition;
    partition.name = "sdb1";
    partition.mount_point = "/mnt/usb";
    device.partitions.push_back(partition);

    const std::vector<MountEntry> found =
        booteable::mounts_for_device(entries, device);
    CHECK_EQ(found.size(), std::size_t{1});
    if (!found.empty()) {
        CHECK_EQ(found[0].mount_point, std::string("/mnt/usb"));
    }
}

} // namespace

void run_mount_manager_tests() {
    test_unmounts_only_target_deepest_first();
    test_unmount_failure_stops();
    test_unmount_verified_after();
    test_no_mounts_is_success();
    test_mounts_by_mount_point_fallback();
}
