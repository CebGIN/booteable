#include "image/BootImageBuilder.hpp"
#include "image/ImageLayout.hpp"
#include "image/ImageVerifier.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "EmbeddedBootImage.hpp"
#include "TestHarness.hpp"
#include "core/Sha256.hpp"

namespace {

std::filesystem::path temp_image(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

std::string file_sha256(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<char> buffer(1024 * 1024);
    booteable::Sha256 hash;
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = file.gcount();
        if (count > 0) {
            hash.update(buffer.data(), static_cast<std::size_t>(count));
        }
    }
    return hash.finish_hex();
}

bool build(const std::filesystem::path& path, booteable::ImageBuildResult& out) {
    booteable::BootImageSpec spec;
    spec.disk_bytes = 80 * booteable::kMib;
    spec.logical_sector = 512;
    spec.volume_label = "BOOTEABLE";
    out = booteable::build_boot_image(path, spec, booteable::kBootEfi,
                                      booteable::kBootEfiSize);
    return out.ok;
}

std::uint8_t read_byte(const std::filesystem::path& path, std::uint64_t offset) {
    std::ifstream file(path, std::ios::binary);
    file.seekg(static_cast<std::streamoff>(offset));
    char value = 0;
    file.read(&value, 1);
    return static_cast<std::uint8_t>(value);
}

void write_byte(const std::filesystem::path& path, std::uint64_t offset,
                std::uint8_t value) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(static_cast<std::streamoff>(offset));
    file.write(reinterpret_cast<const char*>(&value), 1);
}

bool corrupt_fails(const std::filesystem::path& path, std::uint64_t offset,
                   std::uint8_t mask, const char* what) {
    const std::uint8_t original = read_byte(path, offset);
    write_byte(path, offset, static_cast<std::uint8_t>(original ^ mask));

    booteable::ImageManifest manifest;
    std::string error;
    const booteable::ImageVerifier verifier;
    const bool verified = verifier.verify(path, manifest, error);
    if (verified) {
        std::cerr << "La corrupción de " << what << " no fue detectada.\n";
    }
    if (error.empty()) {
        std::cerr << "La corrupción de " << what << " no dio un mensaje.\n";
    }

    write_byte(path, offset, original);
    return !verified;
}

} // namespace

void run_image_roundtrip_tests() {
    const std::filesystem::path image = temp_image("booteable-test-roundtrip.img");
    const std::filesystem::path second =
        temp_image("booteable-test-roundtrip-2.img");

    booteable::ImageBuildResult built;
    CHECK(build(image, built));
    if (!built.ok) {
        std::cerr << "build: " << built.error << "\n";
        return;
    }

    CHECK_EQ(built.manifest.disk_bytes, 80 * booteable::kMib);
    CHECK_EQ(built.manifest.regions.size(), std::size_t{4});
    CHECK_EQ(built.manifest.regions[0].offset, 0ull);
    CHECK_EQ(built.manifest.regions[0].length, 512ull);
    CHECK_EQ(built.manifest.regions[1].offset, 512ull);
    CHECK_EQ(built.manifest.regions[1].length, 33ull * 512);
    CHECK_EQ(built.manifest.regions[2].offset, 2048ull * 512);
    CHECK_EQ(built.manifest.regions[2].length, 64ull * 1024 * 1024);
    CHECK_EQ(built.manifest.regions[3].offset, 163807ull * 512);
    CHECK_EQ(built.manifest.regions[3].length, 33ull * 512);
    CHECK_EQ(std::filesystem::file_size(image), 80 * booteable::kMib);

    {
        booteable::ImageManifest verified;
        std::string error;
        booteable::ImageVerifier verifier;
        CHECK(verifier.verify(image, verified, error));
        if (!error.empty()) {
            std::cerr << "verify: " << error << "\n";
        }
        CHECK_EQ(verified.manifest_sha256, built.manifest.manifest_sha256);
        CHECK_EQ(booteable::canonical_manifest_hash(verified),
                 built.manifest.manifest_sha256);

        booteable::ImageManifest compared;
        CHECK(verifier.verify_against(image, built.manifest, compared, error));
    }

    // Dos builds de la misma entrada son idénticos: mismo manifiesto.
    {
        booteable::ImageBuildResult again;
        CHECK(build(second, again));
        CHECK_EQ(again.manifest.manifest_sha256,
                 built.manifest.manifest_sha256);
        for (std::size_t index = 0; index < built.manifest.regions.size();
             ++index) {
            CHECK_EQ(again.manifest.regions[index].sha256,
                     built.manifest.regions[index].sha256);
        }
        CHECK_EQ(file_sha256(second), file_sha256(image));
        std::filesystem::remove(second);
    }

    // Un byte alterado en cada región hace fallar verify.
    const std::uint64_t sector = 512;
    const std::uint64_t data_start =
        (32ull + 2ull * built.layout.fat_sectors) * sector;
    CHECK(corrupt_fails(image, 300, 0x01, "el código del MBR"));
    CHECK(corrupt_fails(image, 510, 0x01, "la firma del MBR"));
    CHECK(corrupt_fails(image, 512 + 16, 0x01, "el CRC de la GPT primaria"));
    CHECK(corrupt_fails(image, 1024, 0x01, "la entrada GPT"));
    CHECK(corrupt_fails(image, 2048ull * 512 + 32ull * 512 + 10 * 4, 0x01,
                        "la FAT"));
    CHECK(corrupt_fails(image, 2048ull * 512 + data_start, 0x01,
                        "el directorio raíz"));
    CHECK(corrupt_fails(image, 2048ull * 512 + data_start + 3 * sector, 0x01,
                        "el contenido del EFI"));
    CHECK(corrupt_fails(image, 163839ull * 512 + 16, 0x01,
                        "el CRC de la GPT de respaldo"));

    std::filesystem::remove(image);
}
