#include "zlib_wrapper.h"

#include <zlib.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

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

    return out;
}

std::vector<std::byte> Decode(const std::vector<std::byte>& in, const size_t out_size) {
    Bytef *tmp = new Bytef[out_size];

    int rc = uncompress(
        tmp,
        reinterpret_cast<uLongf *>(const_cast<size_t *>(&out_size)),
        reinterpret_cast<const Bytef *>(in.data()),
        static_cast<uLong>(in.size())
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
