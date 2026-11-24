#include "image/GptWriter.hpp"
#include "image/ImageLayout.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "TestHarness.hpp"
#include "core/Bytes.hpp"
#include "core/Crc32.hpp"

namespace {

booteable::ImageLayout layout_512() {
    booteable::BootImageSpec spec;
    spec.disk_bytes = 80 * booteable::kMib;
    spec.logical_sector = 512;
    spec.volume_label = "BOOTEABLE";
    const booteable::LayoutResult result = booteable::compute_layout(spec);
    return result.layout;
}

} // namespace

void run_gpt_tests() {
    using booteable::GptGuids;
    using booteable::ImageLayout;
    using booteable::default_gpt_guids;
    using booteable::esp_type_guid;
    using booteable::make_backup_gpt;
    using booteable::make_primary_gpt;
    using booteable::make_protective_mbr;
    using booteable::parse_gpt_header;

    const ImageLayout layout = layout_512();
    const GptGuids guids = default_gpt_guids();

    {
        const std::vector<std::uint8_t> mbr = make_protective_mbr(layout);
        CHECK_EQ(mbr.size(), std::size_t{512});
        CHECK_EQ(mbr[510], 0x55u);
        CHECK_EQ(mbr[511], 0xAAu);
        CHECK_EQ(mbr[446], 0x00u);
        CHECK_EQ(mbr[450], 0xEEu);
        CHECK_EQ(booteable::read_le32(mbr.data() + 446 + 8), 1u);
        CHECK_EQ(booteable::read_le32(mbr.data() + 446 + 12), 163839u);
        CHECK_EQ(mbr[451], 0xFFu);
        CHECK_EQ(mbr[452], 0xFFu);
        CHECK_EQ(mbr[453], 0xFFu);
    }

    {
        const std::vector<std::uint8_t> primary =
            make_primary_gpt(layout, guids, "BOOTEABLE");
        CHECK_EQ(primary.size(), std::size_t{33 * 512});
        CHECK(std::memcmp(primary.data(), "EFI PART", 8) == 0);

        booteable::GptHeaderInfo header;
        std::string error;
        CHECK(parse_gpt_header(primary.data(), 512, header, error));
        CHECK_EQ(header.current_lba, 1ull);
        CHECK_EQ(header.backup_lba, 163839ull);
        CHECK_EQ(header.first_usable_lba, 34ull);
        CHECK_EQ(header.last_usable_lba, 163806ull);
        CHECK_EQ(header.entries_lba, 2ull);
        CHECK_EQ(header.entry_count, 128u);
        CHECK_EQ(header.entry_size, 128u);
        CHECK(!booteable::guid_is_zero(header.disk_guid));

        const std::uint8_t* entries = primary.data() + 512;
        CHECK(booteable::gpt_entries_crc_ok(entries, 128 * 128,
                                            header.entries_crc32));
        CHECK(std::memcmp(entries, esp_type_guid().bytes, 16) == 0);
        CHECK_EQ(booteable::read_le64(entries + 32), 2048ull);
        CHECK_EQ(booteable::read_le64(entries + 40), 133119ull);
        CHECK_EQ(entries[56], 'B');
        CHECK_EQ(entries[57], 0);
        CHECK_EQ(entries[58], 'O');
        CHECK_EQ(entries[59], 0);
    }

    {
        const std::vector<std::uint8_t> backup =
            make_backup_gpt(layout, guids, "BOOTEABLE");
        CHECK_EQ(backup.size(), std::size_t{33 * 512});
        const std::uint8_t* header_bytes = backup.data() + 32 * 512;
        booteable::GptHeaderInfo header;
        std::string error;
        CHECK(parse_gpt_header(header_bytes, 512, header, error));
        CHECK_EQ(header.current_lba, 163839ull);
        CHECK_EQ(header.backup_lba, 1ull);
        CHECK_EQ(header.entries_lba, 163807ull);
        CHECK(booteable::gpt_entries_crc_ok(backup.data(), 128 * 128,
                                            header.entries_crc32));
    }

    {
        booteable::BootImageSpec spec;
        spec.disk_bytes = 320 * booteable::kMib;
        spec.logical_sector = 4096;
        spec.volume_label = "BOOTEABLE";
        const booteable::LayoutResult result = booteable::compute_layout(spec);
        CHECK(result.ok);
        const std::vector<std::uint8_t> primary =
            make_primary_gpt(result.layout, guids, "BOOTEABLE");
        CHECK_EQ(primary.size(), std::size_t{5 * 4096});
        booteable::GptHeaderInfo header;
        std::string error;
        CHECK(parse_gpt_header(primary.data(), 4096, header, error));
        CHECK_EQ(header.first_usable_lba, 6ull);
        CHECK_EQ(header.last_usable_lba, 81914ull);
        const std::uint8_t* entries = primary.data() + 4096;
        CHECK_EQ(booteable::read_le64(entries + 32), 256ull);
        CHECK_EQ(booteable::read_le64(entries + 40), 66815ull);
    }

    {
        booteable::Guid parsed;
        CHECK(booteable::parse_guid(
            "C12A7328-F81F-11D2-BA4B-00A0C93EC93B", parsed));
        CHECK(std::memcmp(parsed.bytes, esp_type_guid().bytes, 16) == 0);
        CHECK_EQ(booteable::format_guid(parsed),
                 std::string("C12A7328-F81F-11D2-BA4B-00A0C93EC93B"));
        CHECK(!booteable::parse_guid("no-es-un-guid", parsed));
    }
}
