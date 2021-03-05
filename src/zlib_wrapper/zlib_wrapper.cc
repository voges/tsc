#include "zlib_wrapper.h"

#include <zlib.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "util/serialize.h"

namespace zlib_wrapper {

std::vector<std::byte> Encode(const std::vector<std::byte>& in, const int level) {
    size_t tmp_size = compressBound(in.size());
    Bytef *tmp = new Bytef[tmp_size];

    int rc = compress2(
        tmp,
        reinterpret_cast<uLongf *>(&tmp_size),
        reinterpret_cast<const Bytef *>(in.data()),
        static_cast<uLong>(in.size()),
        level
    );

    if (rc != Z_OK) {
        throw std::runtime_error("zlib compression failed");
    }

    std::vector<std::byte> out(tmp_size);
    std::transform(tmp, (tmp + tmp_size), out.begin(), [] (Bytef b) { return std::byte(b); });
    delete[] tmp;

    // Append uncompressed size to output
    auto in_size_bytes = util::pack<uint64_t>(static_cast<uint64_t>(in.size()));
    out.insert(out.end(), in_size_bytes.begin(), in_size_bytes.end());

    return out;
}

std::vector<std::byte> Decode(const std::vector<std::byte>& in) {
    // Retrieve uncompressed size from input
    std::vector<std::byte> out_size_bytes((in.end() - sizeof(uint64_t)), in.end());
    size_t out_size = static_cast<size_t>(util::unpack<uint64_t>(out_size_bytes));
    size_t data_size = in.size() - sizeof(uint64_t);

    Bytef *tmp = new Bytef[out_size];

    int rc = uncompress(
        tmp,
        reinterpret_cast<uLongf *>(const_cast<size_t *>(&out_size)),
        reinterpret_cast<const Bytef *>(in.data()),
        static_cast<uLong>(data_size)
    );

    if (rc != Z_OK) {
        throw std::runtime_error("zlib decompression failed");
    }

    std::vector<std::byte> out(out_size);
    std::transform(tmp, (tmp + out_size), out.begin(), [] (Bytef b) { return std::byte(b); });
    delete[] tmp;

    return out;
}

} // namespace zlib_wrapper
