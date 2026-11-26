#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "TestHarness.hpp"
#include "device/DeviceIdentity.hpp"
#include "device/SysfsBlockDeviceSource.hpp"
#include "device/SystemContext.hpp"
#include "domain/Types.hpp"

namespace {

using booteable::DeviceInfo;
using booteable::DeviceTransport;
using booteable::SysfsBlockDeviceSource;
using booteable::SysfsPaths;
using booteable::SystemContext;
using booteable::SystemContextReader;

void write_file(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

struct Fixture {
    std::filesystem::path root;

    explicit Fixture(const std::string& name) {
        root = std::filesystem::temp_directory_path() / name;
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root);
    }

    ~Fixture() {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    std::filesystem::path sys() const { return root / "sys"; }
    std::filesystem::path dev() const { return root / "dev"; }
    std::filesystem::path mountinfo() const { return root / "mountinfo"; }

    SysfsPaths paths() const {
        SysfsPaths paths;
        paths.sysfs = sys();
        paths.dev = dev();
        paths.mountinfo = mountinfo();
        paths.disk_by_id = dev() / "disk" / "by-id";
        return paths;
    }

    void add_usb_disk(bool usb_serial = true) {
        // Estructura real de sysfs: el nodo de bloque cuelga del nodo SCSI, y
        // la serie del pendrive vive en el nodo usb_device (1-1), no en el SCSI.
        const std::filesystem::path usb_root =
            sys() / "devices" / "pci0000:00" / "usb1";
        const std::filesystem::path usb_port = usb_root / "1-1";
        const std::filesystem::path scsi_device =
            usb_port / "1-1:1.0" / "host0" / "target0:0:0" / "0:0:0:0";
        const std::filesystem::path usb_device = scsi_device / "block" / "sdb";
        std::filesystem::create_directories(usb_device);
        std::filesystem::create_directories(sys() / "block");
        std::error_code error;
        std::filesystem::create_directory_symlink(usb_device, sys() / "block" / "sdb",
                                                  error);
        std::filesystem::create_directory_symlink("../../../0:0:0:0",
                                                  usb_device / "device", error);

        write_file(usb_port / "idVendor", "0951\n");
        if (usb_serial) {
            write_file(usb_port / "serial", "1234ABCD\n");
        }
        // El controlador también tiene serie; no debe usarse para el disco.
        write_file(usb_root / "serial", "0000:00:14.0\n");
        write_file(scsi_device / "model", "Generic Flash Disk\n");
        write_file(usb_device / "size", "16777216\n");  // 8 GiB en sectores de 512
        write_file(usb_device / "queue" / "logical_block_size", "512\n");
        write_file(usb_device / "removable", "1\n");
        write_file(usb_device / "ro", "0\n");
        write_file(usb_device / "dev", "8:16\n");
        std::filesystem::create_directories(usb_device / "holders");

        const std::filesystem::path partition = usb_device / "sdb1";
        write_file(partition / "partition", "1\n");
        write_file(partition / "dev", "8:17\n");
        write_file(partition / "size", "2097152\n");  // 1 GiB

        write_file(dev() / "sdb", "");
        write_file(dev() / "sdb1", "");
        std::filesystem::create_directories(dev() / "disk" / "by-id");
        std::filesystem::create_directory_symlink(
            std::filesystem::path("..") / ".." / "sdb",
            dev() / "disk" / "by-id" / "usb-Generic_Flash_Disk_1234", error);
    }

    void add_system_disk() {
        const std::filesystem::path sda = sys() / "block" / "sda";
        write_file(sda / "device" / "model", "System SSD\n");
        write_file(sda / "device" / "serial", "SYSTEM-1\n");
        write_file(sda / "size", "500118192\n");
        write_file(sda / "queue" / "logical_block_size", "512\n");
        write_file(sda / "removable", "0\n");
        write_file(sda / "ro", "0\n");
        write_file(sda / "dev", "8:0\n");
        write_file(sda / "sda1" / "partition", "1\n");
        write_file(sda / "sda1" / "dev", "8:1\n");
        write_file(sda / "sda1" / "size", "1048576\n");
    }

    void add_skipped_devices() {
        std::filesystem::create_directories(sys() / "block" / "loop0");
        std::filesystem::create_directories(sys() / "block" / "zram0");
    }

    void write_mountinfo(const std::string& text) {
        write_file(mountinfo(), text);
    }
};

void test_enumeration() {
    Fixture fixture("booteable-test-sysfs-list");
    fixture.add_usb_disk();
    fixture.add_system_disk();
    fixture.add_skipped_devices();
    fixture.write_mountinfo(
        "36 25 8:17 / /media/user/USB rw,relatime - vfat /dev/sdb1 rw\n"
        "37 25 8:1 / / rw,relatime - ext4 /dev/sda1 rw\n");

    SysfsBlockDeviceSource source(fixture.paths());
    const std::vector<DeviceInfo> devices = source.list();

    CHECK_EQ(devices.size(), std::size_t{2});
    if (devices.size() != 2) {
        return;
    }

    const DeviceInfo* sdb_found = nullptr;
    const DeviceInfo* sda_found = nullptr;
    const std::string sdb_path = (fixture.dev() / "sdb").string();
    const std::string sda_path = (fixture.dev() / "sda").string();
    for (const DeviceInfo& device : devices) {
        if (device.path == sdb_path) {
            sdb_found = &device;
        } else if (device.path == sda_path) {
            sda_found = &device;
        }
    }
    CHECK(sdb_found != nullptr);
    CHECK(sda_found != nullptr);
    if (sdb_found == nullptr || sda_found == nullptr) {
        return;
    }
    const DeviceInfo& sdb = *sdb_found;
    const DeviceInfo& sda = *sda_found;
    CHECK_EQ(sdb.path, (fixture.dev() / "sdb").string());
    CHECK_EQ(sdb.model, std::string("Generic Flash Disk"));
    CHECK_EQ(sdb.serial, std::string("1234ABCD"));
    CHECK_EQ(sdb.size_bytes, 8ull * 1024ull * 1024ull * 1024ull);
    CHECK_EQ(sdb.logical_sector, 512u);
    CHECK(sdb.removable);
    CHECK(sdb.transport == DeviceTransport::Usb);
    CHECK_EQ(sdb.dev_id, std::string("8:16"));
    CHECK_EQ(sdb.by_id,
             (fixture.dev() / "disk" / "by-id" /
              "usb-Generic_Flash_Disk_1234")
                 .string());
    CHECK_EQ(sdb.partitions.size(), std::size_t{1});
    if (!sdb.partitions.empty()) {
        CHECK_EQ(sdb.partitions[0].name, std::string("sdb1"));
        CHECK_EQ(sdb.partitions[0].dev_id, std::string("8:17"));
        CHECK_EQ(sdb.partitions[0].mount_point,
                 std::string("/media/user/USB"));
        CHECK_EQ(sdb.partitions[0].size_bytes, 1024ull * 1024ull * 1024ull);
    }

    CHECK_EQ(sda.path, (fixture.dev() / "sda").string());
    CHECK(!sda.removable);
    CHECK(sda.transport == DeviceTransport::Sata);
    CHECK(sda.by_id.empty());
}

void test_usb_without_serial_stays_empty() {
    // Si el nodo usb_device no publica serie, la política debe rechazarlo. No
    // se hereda la serie del controlador USB, que comparten todos los puertos.
    Fixture fixture("booteable-test-sysfs-no-serial");
    fixture.add_usb_disk(false);
    fixture.write_mountinfo("");

    const std::vector<DeviceInfo> devices =
        SysfsBlockDeviceSource(fixture.paths()).list();
    CHECK_EQ(devices.size(), std::size_t{1});
    if (!devices.empty()) {
        CHECK(devices[0].serial.empty());
        CHECK(devices[0].transport == DeviceTransport::Usb);
    }
}

void test_refresh() {
    Fixture fixture("booteable-test-sysfs-refresh");
    fixture.add_usb_disk();
    fixture.write_mountinfo("");

    SysfsBlockDeviceSource source(fixture.paths());
    const std::optional<DeviceInfo> refreshed = source.refresh("/dev/sdb");
    CHECK(refreshed.has_value());
    if (refreshed) {
        CHECK_EQ(refreshed->serial, std::string("1234ABCD"));
    }
    CHECK(!source.refresh("/dev/sdz").has_value());
}

void test_system_context() {
    Fixture fixture("booteable-test-sysfs-context");
    fixture.add_usb_disk();
    fixture.add_system_disk();
    fixture.write_mountinfo(
        "36 25 8:17 / /media/user/USB rw,relatime - vfat /dev/sdb1 rw\n"
        "37 25 8:1 / / rw,relatime - ext4 /dev/sda1 rw\n"
        "38 25 8:1 / /boot rw,relatime - ext4 /dev/sda1 rw\n");

    SystemContextReader reader(fixture.sys(), fixture.mountinfo());
    const SystemContext context = reader.read();
    CHECK_EQ(context.root_device, std::string("sda"));
    CHECK_EQ(context.boot_device, std::string("sda"));
    CHECK_EQ(context.mounted_device_names.size(), std::size_t{2});
    CHECK_EQ(context.mounted_device_names[0], std::string("sda"));
    CHECK_EQ(context.mounted_device_names[1], std::string("sdb"));
}

void test_transport_from_uevent() {
    // Un disco cuyo enlace no pasa por /usb se reconoce igual por el controlador
    // USB que informa sysfs en device/uevent.
    Fixture fixture("booteable-test-sysfs-uevent");
    const std::filesystem::path disk = fixture.sys() / "block" / "sdc";
    write_file(disk / "device" / "uevent",
               "DEVTYPE=disk\nDRIVER=usb-storage\nMAJOR=8\nMINOR=32\n");
    write_file(disk / "size", "16777216\n");
    write_file(disk / "dev", "8:32\n");
    fixture.write_mountinfo("");

    const std::vector<DeviceInfo> devices =
        SysfsBlockDeviceSource(fixture.paths()).list();
    CHECK_EQ(devices.size(), std::size_t{1});
    if (!devices.empty()) {
        CHECK(devices[0].transport == DeviceTransport::Usb);
    }
}

void test_mountinfo_unescape() {
    CHECK_EQ(booteable::unescape_mountinfo_field("/media/My\\040USB"),
             std::string("/media/My USB"));
    CHECK_EQ(booteable::unescape_mountinfo_field("sin\\011escape"),
             std::string("sin\tescape"));
}

void test_disk_name_from_major_minor() {
    Fixture fixture("booteable-test-sysfs-major-minor");
    fixture.add_usb_disk();
    fixture.add_system_disk();

    CHECK_EQ(booteable::disk_name_for_major_minor(fixture.sys(), "8:17"),
             std::string("sdb"));
    CHECK_EQ(booteable::disk_name_for_major_minor(fixture.sys(), "8:1"),
             std::string("sda"));
    CHECK_EQ(booteable::disk_name_for_major_minor(fixture.sys(), "9:9"),
             std::string(""));
}

} // namespace

void run_sysfs_source_tests() {
    test_enumeration();
    test_usb_without_serial_stays_empty();
    test_refresh();
    test_system_context();
    test_transport_from_uevent();
    test_mountinfo_unescape();
    test_disk_name_from_major_minor();
}
