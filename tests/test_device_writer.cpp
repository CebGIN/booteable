#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "TestHarness.hpp"
#include "device/FaultyDeviceWriter.hpp"
#include "device/FileDeviceWriter.hpp"
#include "device/PosixDeviceWriter.hpp"
#include "domain/Types.hpp"

namespace {

using booteable::DeviceInfo;
using booteable::FaultyDeviceWriter;
using booteable::FileDeviceWriter;
using booteable::PosixDeviceWriter;

std::filesystem::path temp_file(const std::string& name) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / name;
    std::error_code error;
    std::filesystem::remove(path, error);
    return path;
}

DeviceInfo make_file_device(const std::filesystem::path& path,
                            std::uint64_t size_bytes) {
    DeviceInfo device;
    device.path = path.string();
    device.size_bytes = size_bytes;
    device.logical_sector = 512;
    device.serial = "regular-file";
    device.by_id = path.string();
    return device;
}

void test_file_writer_roundtrip() {
    const std::filesystem::path path = temp_file("booteable-test-writer.img");
    const std::uint64_t size = 8192;

    FileDeviceWriter writer(true, true);
    std::string error;
    CHECK(writer.open(make_file_device(path, size), error));
    if (!error.empty()) {
        return;
    }
    CHECK(writer.is_open());
    CHECK_EQ(writer.size(), size);
    CHECK_EQ(writer.sector_size(), 512u);
    CHECK_EQ(writer.bytes_written(), 0ull);

    std::vector<std::byte> data(4096);
    for (std::size_t index = 0; index < data.size(); ++index) {
        data[index] = static_cast<std::byte>(index % 251);
    }

    CHECK(writer.write(data.data(), data.size(), error));
    CHECK_EQ(writer.bytes_written(), data.size());
    CHECK(writer.write(data.data(), data.size(), error));
    CHECK_EQ(writer.bytes_written(), size);
    CHECK(writer.sync(error));

    std::vector<std::byte> read_back(data.size());
    CHECK(writer.read_at(0, read_back.data(), read_back.size(), error));
    CHECK(read_back == data);
    CHECK(writer.read_at(4096, read_back.data(), read_back.size(), error));
    CHECK(read_back == data);

    writer.close();
    CHECK(!writer.is_open());
    CHECK_EQ(std::filesystem::file_size(path), size);

    std::filesystem::remove(path);
}

void test_writer_rejects_overflow() {
    const std::filesystem::path path =
        temp_file("booteable-test-writer-overflow.img");
    const std::uint64_t size = 4096;

    FileDeviceWriter writer(true, true);
    std::string error;
    CHECK(writer.open(make_file_device(path, size), error));

    std::vector<std::byte> data(4096);
    CHECK(writer.write(data.data(), data.size(), error));
    CHECK(!writer.write(data.data(), data.size(), error));
    CHECK_EQ(writer.last_errno(), ENOSPC);

    std::vector<std::byte> read_back(8);
    CHECK(!writer.read_at(size, read_back.data(), read_back.size(), error));
    CHECK_EQ(writer.last_errno(), EINVAL);

    writer.close();
    std::filesystem::remove(path);
}

void test_posix_writer_requires_existing_file() {
    const std::filesystem::path path =
        temp_file("booteable-test-writer-missing.img");

    PosixDeviceWriter writer;
    std::string error;
    CHECK(!writer.open(make_file_device(path, 4096), error));
    CHECK(!error.empty());
    CHECK_EQ(writer.last_errno(), ENOENT);
    CHECK(!writer.is_open());
}

void test_posix_writer_existing_file() {
    const std::filesystem::path path =
        temp_file("booteable-test-writer-existing.img");
    {
        std::ofstream file(path, std::ios::binary);
        file << std::string(4096, '\0');
    }

    PosixDeviceWriter writer;
    std::string error;
    CHECK(writer.open(make_file_device(path, 4096), error));
    CHECK_EQ(writer.size(), 4096ull);
    CHECK_EQ(writer.sector_size(), 512u);
    CHECK(!writer.is_block_device());

    std::vector<std::byte> data(512, std::byte{0x5a});
    CHECK(writer.write(data.data(), data.size(), error));
    CHECK(writer.sync(error));
    writer.close();

    std::filesystem::remove(path);
}

void test_faulty_writer_injects_enodev() {
    const std::filesystem::path path =
        temp_file("booteable-test-writer-faulty.img");

    FileDeviceWriter inner(true, true);
    std::string error;
    CHECK(inner.open(make_file_device(path, 4096), error));

    FaultyDeviceWriter faulty(inner);
    faulty.fail_after(1024, ENODEV);

    std::vector<std::byte> data(512, std::byte{0x11});
    CHECK(faulty.write(data.data(), data.size(), error));
    CHECK(faulty.write(data.data(), data.size(), error));
    CHECK(!faulty.write(data.data(), data.size(), error));
    CHECK_EQ(faulty.last_errno(), ENODEV);
    CHECK_EQ(faulty.bytes_written(), 1024ull);

    faulty.close();
    std::filesystem::remove(path);
}

void test_faulty_writer_injects_sync_error() {
    const std::filesystem::path path =
        temp_file("booteable-test-writer-sync.img");

    FileDeviceWriter inner(true, true);
    std::string error;
    CHECK(inner.open(make_file_device(path, 4096), error));

    FaultyDeviceWriter faulty(inner);
    faulty.fail_sync(EIO);
    CHECK(!faulty.sync(error));
    CHECK_EQ(faulty.last_errno(), EIO);

    faulty.close();
    std::filesystem::remove(path);
}

} // namespace

void run_device_writer_tests() {
    test_file_writer_roundtrip();
    test_writer_rejects_overflow();
    test_posix_writer_requires_existing_file();
    test_posix_writer_existing_file();
    test_faulty_writer_injects_enodev();
    test_faulty_writer_injects_sync_error();
}
