#include "core/Crc32.hpp"
#include "core/Sha256.hpp"

#include <cstdint>
#include <string>

#include "TestHarness.hpp"

void run_core_tests() {
    using booteable::Crc32;
    using booteable::Sha256;
    using booteable::crc32;

    // Vectores conocidos de CRC32 (IEEE 802.3, polinomio 0xEDB88320).
    CHECK_EQ(crc32("", 0), 0u);
    CHECK_EQ(crc32("123456789", 9), 0xCBF43926u);
    CHECK_EQ(crc32("The quick brown fox jumps over the lazy dog", 43),
             0x414FA339u);

    {
        Crc32 incremental;
        incremental.update("1234", 4);
        incremental.update("56789", 5);
        CHECK_EQ(incremental.value(), 0xCBF43926u);
    }

    // Vectores conocidos de SHA-256.
    CHECK_EQ(Sha256::hex("", 0),
             std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(Sha256::hex("abc", 3),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK_EQ(Sha256::hex("The quick brown fox jumps over the lazy dog", 43),
             std::string("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"));

    {
        const std::string text = "The quick brown fox jumps over the lazy dog";
        Sha256 incremental;
        incremental.update(text.data(), 10);
        incremental.update(text.data() + 10, 20);
        incremental.update(text.data() + 30, text.size() - 30);
        CHECK_EQ(incremental.finish_hex(),
                 std::string("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"));
    }

    {
        // Muchos ceros: mismo resultado que hashear un bloque de ceros.
        Sha256 zeros;
        zeros.update_zeros(100000);
        Sha256 explicit_zeros;
        const std::string block(100000, '\0');
        explicit_zeros.update(block.data(), block.size());
        CHECK_EQ(zeros.finish_hex(), explicit_zeros.finish_hex());
    }
}
