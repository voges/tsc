#include <gtest/gtest.h>

#include "util/serialize.h"

#include <cstddef>

TEST(Serialize, PackUint8) {
    uint8_t in = 0xFF;
    std::vector<std::byte> serial = util::pack<uint8_t>(in);
    EXPECT_EQ(serial.size(), sizeof(uint8_t));
    EXPECT_EQ(static_cast<unsigned char>(serial[0]), 0xFF);
}

TEST(Serialize, PackUint16) {
    uint16_t in = 0xFFEE;
    std::vector<std::byte> serial = util::pack<uint16_t>(in);
    EXPECT_EQ(serial.size(), sizeof(uint16_t));
    EXPECT_EQ(static_cast<unsigned char>(serial[0]), 0xEE);
    EXPECT_EQ(static_cast<unsigned char>(serial[1]), 0xFF);
}

TEST(Serialize, PackUint32) {
    uint32_t in = 0xFFEEDDCC;
    std::vector<std::byte> serial = util::pack<uint32_t>(in);
    EXPECT_EQ(serial.size(), sizeof(uint32_t));
    EXPECT_EQ(static_cast<unsigned char>(serial[0]), 0xCC);
    EXPECT_EQ(static_cast<unsigned char>(serial[1]), 0xDD);
    EXPECT_EQ(static_cast<unsigned char>(serial[2]), 0xEE);
    EXPECT_EQ(static_cast<unsigned char>(serial[3]), 0xFF);
}

TEST(Serialize, PackUint64) {
    uint64_t in = 0xFFEEDDCCBBAA9988;
    std::vector<std::byte> serial = util::pack<uint64_t>(in);
    EXPECT_EQ(serial.size(), sizeof(uint64_t));
    EXPECT_EQ(static_cast<unsigned char>(serial[0]), 0x88);
    EXPECT_EQ(static_cast<unsigned char>(serial[1]), 0x99);
    EXPECT_EQ(static_cast<unsigned char>(serial[2]), 0xAA);
    EXPECT_EQ(static_cast<unsigned char>(serial[3]), 0xBB);
    EXPECT_EQ(static_cast<unsigned char>(serial[4]), 0xCC);
    EXPECT_EQ(static_cast<unsigned char>(serial[5]), 0xDD);
    EXPECT_EQ(static_cast<unsigned char>(serial[6]), 0xEE);
    EXPECT_EQ(static_cast<unsigned char>(serial[7]), 0xFF);
}

TEST(Serialize, UnpackUint8) {
    std::vector<std::byte> serial = { std::byte{0xFF} };
    uint8_t out = util::unpack<uint8_t>(serial);
    EXPECT_EQ(out, 0xFF);
}

TEST(Serialize, UnpackUint16) {
    std::vector<std::byte> serial = {
        std::byte{0xEE},
        std::byte{0xFF}
    };
    uint16_t out = util::unpack<uint16_t>(serial);
    EXPECT_EQ(out, 0xFFEE);
}

TEST(Serialize, UnpackUint32) {
    std::vector<std::byte> serial = {
        std::byte{0xCC},
        std::byte{0xDD},
        std::byte{0xEE},
        std::byte{0xFF}
    };
    uint32_t out = util::unpack<uint32_t>(serial);
    EXPECT_EQ(out, 0xFFEEDDCC);
}

TEST(Serialize, UnpackUint64) {
    std::vector<std::byte> serial = {
        std::byte{0x88},
        std::byte{0x99},
        std::byte{0xAA},
        std::byte{0xBB},
        std::byte{0xCC},
        std::byte{0xDD},
        std::byte{0xEE},
        std::byte{0xFF}
    };
    uint64_t out = util::unpack<uint64_t>(serial);
    EXPECT_EQ(out, 0xFFEEDDCCBBAA9988);
}

TEST(Serialize, RoundtripUint8) {
    uint8_t in = 0xFF;
    uint8_t out = util::unpack<uint8_t>(util::pack<uint8_t>(in));
    EXPECT_EQ(in, out);
}

TEST(Serialize, RoundtripUint16) {
    uint16_t in = 0xFFEE;
    uint16_t out = util::unpack<uint16_t>(util::pack<uint16_t>(in));
    EXPECT_EQ(in, out);
}

TEST(Serialize, RoundtripUint32) {
    uint32_t in = 0xFFEEDDCC;
    uint32_t out = util::unpack<uint32_t>(util::pack<uint32_t>(in));
    EXPECT_EQ(in, out);
}

TEST(Serialize, RoundtripUint64) {
    uint64_t in = 0xFFEEDDCCBBAA9988;
    uint64_t out = util::unpack<uint64_t>(util::pack<uint64_t>(in));
    EXPECT_EQ(in, out);
}
