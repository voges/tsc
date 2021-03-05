#include <gtest/gtest.h>

#include "zlib_wrapper/zlib_wrapper.h"

#include <limits>
#include <random>

static std::vector<std::byte> UniformDist(const size_t size) {
    std::random_device r;
    std::default_random_engine Generator(r());
    std::uniform_int_distribution<int> UniformDist(std::numeric_limits<unsigned char>::min(), std::numeric_limits<unsigned char>::max());
    std::vector<std::byte> vec(size);
    std::generate(vec.begin(), vec.end(), [&UniformDist, &Generator] () { return std::byte(UniformDist(Generator)); });
    return vec;
}

TEST(ZlibWrapper, EmptyRoundtrip) {
    std::vector<std::byte> in;
    auto dec = zlib_wrapper::Decode(zlib_wrapper::Encode(in), in.size());
    EXPECT_EQ(in, dec);
}

TEST(ZlibWrapper, LonelyByteRoundtrip) {
    std::vector<std::byte> in = { std::byte{0} };
    auto dec = zlib_wrapper::Decode(zlib_wrapper::Encode(in), in.size());
    EXPECT_EQ(in, dec);
}

TEST(ZlibWrapper, RandomRoundtrip) {
    auto in = UniformDist(1 * 1024 * 1024); // 1 MiB
    auto dec = zlib_wrapper::Decode(zlib_wrapper::Encode(in), in.size());
    EXPECT_EQ(in, dec);
}
