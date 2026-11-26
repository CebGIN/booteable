#include "image/BootImageBuilder.hpp"
#include "image/ImageLayout.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "EmbeddedBootImage.hpp"
#include "TestHarness.hpp"
#include "core/Bytes.hpp"
#include "core/Sha256.hpp"

namespace {

std::vector<std::uint8_t> read_region(const std::filesystem::path& path,
                                      std::uint64_t offset,
                                      std::size_t length) {
    std::vector<std::uint8_t> data(length, 0);
    std::ifstream file(path, std::ios::binary);
    file.seekg(static_cast<std::streamoff>(offset));
    file.read(reinterpret_cast<char*>(data.data()),
              static_cast<std::streamsize>(length));
    return data;
}

std::filesystem::path temp_image(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

bool build_ok(const std::filesystem::path& path, std::uint64_t disk_mib,
              std::uint32_t sector, booteable::ImageBuildResult& result) {
    booteable::BootImageSpec spec;
    spec.disk_bytes = disk_mib * booteable::kMib;
    spec.logical_sector = sector;
    spec.volume_label = "BOOTEABLE";
    result = booteable::build_boot_image(path, spec, booteable::kBootEfi,
                                         booteable::kBootEfiSize);
    return result.ok;
}

std::uint32_t fat_entry(const std::vector<std::uint8_t>& fat,
                        std::uint32_t cluster) {
    return booteable::read_le32(fat.data() + cluster * 4) & 0x0FFFFFFFu;
}

} // namespace

void run_fat32_tests() {
    const std::filesystem::path image = temp_image("booteable-test-fat32.img");
    booteable::ImageBuildResult result;

    CHECK(build_ok(image, 80, 512, result));
    if (!result.ok) {
        std::filesystem::remove(image);
        return;
    }

    const std::uint64_t sector = 512;
    const std::uint64_t esp_offset = result.layout.esp_first_lba * sector;
    const std::uint64_t fat_sectors = result.layout.fat_sectors;
    const std::uint64_t data_start =
        (32 + 2 * fat_sectors) * sector;

    {
        const std::vector<std::uint8_t> boot =
            read_region(image, esp_offset, 512);
        CHECK_EQ(booteable::read_le16(boot.data() + 11), 512u);
        CHECK_EQ(boot[13], 1u);
        CHECK_EQ(booteable::read_le16(boot.data() + 14), 32u);
        CHECK_EQ(boot[16], 2u);
        CHECK_EQ(booteable::read_le16(boot.data() + 17), 0u);
        CHECK_EQ(boot[21], 0xF8u);
        CHECK_EQ(booteable::read_le32(boot.data() + 28),
                 static_cast<std::uint32_t>(result.layout.esp_first_lba));
        CHECK_EQ(booteable::read_le32(boot.data() + 32),
                 static_cast<std::uint32_t>(result.layout.esp_sectors));
        CHECK_EQ(booteable::read_le32(boot.data() + 36),
                 static_cast<std::uint32_t>(fat_sectors));
        CHECK_EQ(booteable::read_le32(boot.data() + 44), 2u);
        CHECK_EQ(booteable::read_le16(boot.data() + 48), 1u);
        CHECK_EQ(booteable::read_le16(boot.data() + 50), 6u);
        CHECK_EQ(boot[66], 0x29u);
        CHECK(std::string(reinterpret_cast<const char*>(boot.data() + 71),
                          11) == "BOOTEABLE  ");
        CHECK(std::string(reinterpret_cast<const char*>(boot.data() + 82),
                          8) == "FAT32   ");
        CHECK_EQ(boot[510], 0x55u);
        CHECK_EQ(boot[511], 0xAAu);
    }

    {
        const std::vector<std::uint8_t> fsinfo =
            read_region(image, esp_offset + 512, 512);
        const std::vector<std::uint8_t> fsinfo_copy =
            read_region(image, esp_offset + 7 * 512, 512);
        CHECK(fsinfo == fsinfo_copy);
        CHECK_EQ(booteable::read_le32(fsinfo.data()), 0x41615252u);
        CHECK_EQ(booteable::read_le32(fsinfo.data() + 484), 0x61417272u);
        CHECK_EQ(booteable::read_le32(fsinfo.data() + 508), 0xAA550000u);

        const std::uint32_t file_clusters = static_cast<std::uint32_t>(
            (booteable::kBootEfiSize + 511) / 512);
        const std::uint32_t used = 3 + file_clusters;
        CHECK_EQ(booteable::read_le32(fsinfo.data() + 488),
                 result.layout.cluster_count - used);
        CHECK_EQ(booteable::read_le32(fsinfo.data() + 492),
                 5u + file_clusters);
    }

    {
        const std::vector<std::uint8_t> fat1 =
            read_region(image, esp_offset + 32 * sector,
                        static_cast<std::size_t>(fat_sectors * sector));
        const std::vector<std::uint8_t> fat2 =
            read_region(image, esp_offset + (32 + fat_sectors) * sector,
                        static_cast<std::size_t>(fat_sectors * sector));
        CHECK(fat1 == fat2);
        CHECK_EQ(booteable::read_le32(fat1.data()), 0x0FFFFFF8u);
        CHECK_EQ(booteable::read_le32(fat1.data() + 4), 0x0FFFFFFFu);
        CHECK_EQ(fat_entry(fat1, 2), 0x0FFFFFFFu);
        CHECK_EQ(fat_entry(fat1, 3), 0x0FFFFFFFu);
        CHECK_EQ(fat_entry(fat1, 4), 0x0FFFFFFFu);

        const std::uint32_t file_clusters = static_cast<std::uint32_t>(
            (booteable::kBootEfiSize + 511) / 512);
        for (std::uint32_t index = 0; index < file_clusters; ++index) {
            const std::uint32_t cluster = 5 + index;
            CHECK_EQ(fat_entry(fat1, cluster),
                     index + 1 == file_clusters ? 0x0FFFFFFFu : cluster + 1);
        }
    }

    {
        const std::vector<std::uint8_t> root =
            read_region(image, esp_offset + data_start, 512);
        CHECK(std::string(reinterpret_cast<const char*>(root.data()), 11) ==
              "BOOTEABLE  ");
        CHECK_EQ(root[11], 0x08u);
        CHECK_EQ(booteable::read_le16(root.data() + 16), 0x5C21u);
        CHECK(std::string(reinterpret_cast<const char*>(root.data() + 32),
                          11) == "EFI        ");
        CHECK_EQ(root[32 + 11], 0x10u);
        CHECK_EQ(booteable::read_le16(root.data() + 32 + 26), 3u);
    }

    {
        const std::vector<std::uint8_t> efi_dir =
            read_region(image, esp_offset + data_start + 512, 512);
        CHECK(std::string(reinterpret_cast<const char*>(efi_dir.data()), 11) ==
              ".          ");
        CHECK(std::string(reinterpret_cast<const char*>(efi_dir.data() + 32),
                          11) == "..         ");
        CHECK(std::string(reinterpret_cast<const char*>(efi_dir.data() + 64),
                          11) == "BOOT       ");
        CHECK_EQ(booteable::read_le16(efi_dir.data() + 64 + 26), 4u);

        const std::vector<std::uint8_t> boot_dir =
            read_region(image, esp_offset + data_start + 2 * 512, 512);
        CHECK(std::string(reinterpret_cast<const char*>(boot_dir.data() + 64),
                          11) == "BOOTX64 EFI");
        CHECK_EQ(boot_dir[64 + 11], 0x20u);
        CHECK_EQ(booteable::read_le16(boot_dir.data() + 64 + 26), 5u);
        CHECK_EQ(booteable::read_le32(boot_dir.data() + 64 + 28),
                 static_cast<std::uint32_t>(booteable::kBootEfiSize));
    }

    {
        const std::vector<std::uint8_t> content =
            read_region(image, esp_offset + data_start + 3 * 512,
                        booteable::kBootEfiSize);
        CHECK_EQ(content.size(), booteable::kBootEfiSize);
        CHECK(std::memcmp(content.data(), booteable::kBootEfi,
                          booteable::kBootEfiSize) == 0);
        CHECK_EQ(booteable::sha256_hex(content.data(), content.size()),
                 std::string(booteable::kBootEfiSha256));
    }

    std::filesystem::remove(image);

    // La misma estructura con sector de 4096 y ESP de 260 MiB.
    const std::filesystem::path image4k = temp_image("booteable-test-fat32-4kn.img");
    booteable::ImageBuildResult result4k;
    CHECK(build_ok(image4k, 320, 4096, result4k));
    if (result4k.ok) {
        const std::uint64_t esp4k = result4k.layout.esp_first_lba * 4096;
        const std::vector<std::uint8_t> boot =
            read_region(image4k, esp4k, 4096);
        CHECK_EQ(booteable::read_le16(boot.data() + 11), 4096u);
        CHECK_EQ(booteable::read_le32(boot.data() + 36),
                 result4k.layout.fat_sectors);
        CHECK_EQ(boot[510], 0x55u);
        CHECK_EQ(boot[511], 0xAAu);

        const std::uint64_t data4k =
            (32 + 2ull * result4k.layout.fat_sectors) * 4096;
        const std::vector<std::uint8_t> boot_dir =
            read_region(image4k, esp4k + data4k + 2 * 4096, 4096);
        CHECK(std::string(reinterpret_cast<const char*>(boot_dir.data() + 64),
                          11) == "BOOTX64 EFI");
        CHECK_EQ(booteable::read_le32(boot_dir.data() + 64 + 28),
                 static_cast<std::uint32_t>(booteable::kBootEfiSize));
    }
    std::filesystem::remove(image4k);
}
