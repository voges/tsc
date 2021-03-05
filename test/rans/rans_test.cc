#include <gtest/gtest.h>

#include "rans/rans.h"

#include <limits>
#include <random>

// TODO: templatize dist functions and move them to separate file
static std::vector<std::byte> UniformDist(const size_t size) {
    std::random_device r;
    std::default_random_engine Generator(r());
    std::uniform_int_distribution<int> UniformDist(std::numeric_limits<unsigned char>::min(), std::numeric_limits<unsigned char>::max());
    std::vector<std::byte> vec(size);
    std::generate(vec.begin(), vec.end(), [&UniformDist, &Generator] () { return std::byte(UniformDist(Generator)); });
    return vec;
}

static std::vector<std::byte> GeometricDist(const size_t size) {
    std::random_device r;
    std::default_random_engine Generator(r());
    std::geometric_distribution<int> GeometricDist(0.01);
    std::vector<std::byte> vec(size);
    std::generate(vec.begin(), vec.end(), [&GeometricDist, &Generator] () { return std::byte(GeometricDist(Generator)); });
    return vec;
}

TEST(Rans, UniformDistRoundtrips) {
    for (int num_unique_symbols = 1; num_unique_symbols <= 8; num_unique_symbols++) {
        const size_t in_size = 1 * 1024 * 1024; // 1 MiB
        std::vector<std::byte> in = UniformDist(in_size);
        std::transform(in.begin(), in.end(), in.begin(), [num_unique_symbols](std::byte& b){ return std::byte(std::to_integer<int>(b) % num_unique_symbols); });

        auto out = rans::Encode(in);
        auto freqs = rans::CountFreqs(in);
        auto dec = rans::Decode(freqs, out, in_size);

        // TODO: (-log2(num_unique_symbols) / 8)
        std::cout << "out_size/in_size: "
                << static_cast<double>(out.size()) / static_cast<double>(in_size)
                << std::endl;
        EXPECT_EQ(in, dec);
    }
}

TEST(Rans, GeometricDistRoundtrips) {
    // for (int num_unique_symbols = 2; num_unique_symbols <= 256; num_unique_symbols++) {
        const size_t in_size = 1 * 1024 * 1024; // 1 MiB
        std::vector<std::byte> in = GeometricDist(in_size);
        // std::transform(in.begin(), in.end(), in.begin(), [num_unique_symbols](std::byte& b){ return std::byte(std::to_integer<int>(b) % num_unique_symbols); });

        auto out = rans::Encode(in);
        auto freqs = rans::CountFreqs(in);
        auto dec = rans::Decode(freqs, out, in_size);

        std::cout << "out_size/in_size: "
                << static_cast<double>(out.size()) / static_cast<double>(in_size)
                << std::endl;
        EXPECT_EQ(in, dec);
    // }
}
