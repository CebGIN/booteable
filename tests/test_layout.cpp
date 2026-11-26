#include "image/ImageLayout.hpp"

#include <cstdint>

#include "TestHarness.hpp"
#include "domain/Types.hpp"

namespace {

booteable::BootImageSpec make_spec(std::uint64_t disk_mib, std::uint32_t sector,
                                   std::uint64_t esp_mib) {
    booteable::BootImageSpec spec;
    spec.disk_bytes = disk_mib * booteable::kMib;
    spec.logical_sector = sector;
    spec.esp_bytes = esp_mib != 0 ? esp_mib * booteable::kMib : 0;
    spec.volume_label = "BOOTEABLE";
    return spec;
}

} // namespace

void run_layout_tests() {
    using booteable::compute_layout;
    using booteable::FatGeometry;
    using booteable::LayoutResult;

    {
        const LayoutResult result = compute_layout(make_spec(80, 512, 0));
        CHECK(result.ok);
        CHECK_EQ(result.layout.sector_size, 512u);
        CHECK_EQ(result.layout.total_sectors, 163840ull);
        CHECK_EQ(result.layout.entry_sectors, 32u);
        CHECK_EQ(result.layout.first_usable_lba, 34ull);
        CHECK_EQ(result.layout.last_usable_lba, 163806ull);
        CHECK_EQ(result.layout.esp_first_lba, 2048ull);
        CHECK_EQ(result.layout.esp_sectors, 131072ull);
        CHECK_EQ(result.layout.backup_header_lba, 163839ull);
        CHECK_EQ(result.layout.backup_entries_lba, 163807ull);
        CHECK_EQ(result.layout.fat_sectors, 1009u);
        CHECK_EQ(result.layout.cluster_count, 129022u);
    }

    {
        const LayoutResult result = compute_layout(make_spec(64, 512, 0));
        // La ESP automática de 64 MiB no entra en un disco de 64 MiB: se
        // reduce al mayor tamaño posible sin bajar del mínimo de 48 MiB.
        CHECK(result.ok);
        CHECK(result.layout.esp_sectors * 512 < 64 * booteable::kMib);
        CHECK(result.layout.esp_sectors * 512 >= 48 * booteable::kMib);
        CHECK_EQ(result.layout.esp_sectors, 128991ull);
        CHECK_EQ(result.layout.fat_sectors, 993u);
        CHECK_EQ(result.layout.cluster_count, 126973u);
    }

    {
        const LayoutResult result = compute_layout(make_spec(320, 4096, 0));
        CHECK(result.ok);
        CHECK_EQ(result.layout.sector_size, 4096u);
        CHECK_EQ(result.layout.total_sectors, 81920ull);
        CHECK_EQ(result.layout.entry_sectors, 4u);
        CHECK_EQ(result.layout.first_usable_lba, 6ull);
        CHECK_EQ(result.layout.last_usable_lba, 81914ull);
        CHECK_EQ(result.layout.esp_first_lba, 256ull);
        CHECK_EQ(result.layout.esp_sectors, 66560ull);
        CHECK_EQ(result.layout.fat_sectors, 65u);
        CHECK_EQ(result.layout.cluster_count, 66398u);
        CHECK_EQ(result.layout.backup_header_lba, 81919ull);
        CHECK_EQ(result.layout.backup_entries_lba, 81915ull);
    }

    {
        FatGeometry geometry;
        std::string error;
        CHECK(booteable::compute_fat_geometry(131072, 512, geometry, error));
        CHECK_EQ(geometry.fat_sectors, 1009u);
        CHECK_EQ(geometry.cluster_count, 129022u);
    }

    {
        const LayoutResult result = compute_layout(make_spec(260, 4096, 0));
        // La ESP mínima de 260 MiB no entra en un disco de 260 MiB con GPT.
        CHECK(!result.ok);
    }

    {
        const LayoutResult result = compute_layout(make_spec(64, 512, 64));
        CHECK(!result.ok);
        CHECK(!result.error.empty());
    }

    {
        const LayoutResult result = compute_layout(make_spec(64, 512, 47));
        CHECK(!result.ok);
    }

    {
        const LayoutResult result = compute_layout(make_spec(260, 4096, 256));
        CHECK(!result.ok);
    }

    {
        const LayoutResult result = compute_layout(make_spec(512, 1024, 0));
        CHECK(!result.ok);
    }

    {
        booteable::BootImageSpec spec = make_spec(128, 512, 0);
        spec.disk_bytes += 1;
        CHECK(!compute_layout(spec).ok);
    }

    {
        const LayoutResult result = compute_layout(make_spec(8, 512, 0));
        CHECK(!result.ok);
    }

    {
        // Un disco de 1 MiB ni siquiera llega al LBA donde empieza la ESP.
        const LayoutResult result = compute_layout(make_spec(1, 512, 0));
        CHECK(!result.ok);
        CHECK(!result.error.empty());
    }

    {
        booteable::BootImageSpec spec = make_spec(128, 512, 0);
        spec.logical_sector = 2048;
        CHECK(!compute_layout(spec).ok);
    }
}
