#ifndef ZLIB_WRAPPER_ZLIB_WRAPPER_H
#define ZLIB_WRAPPER_ZLIB_WRAPPER_H

#include <zlib.h>

#include <vector>

namespace zlib_wrapper {

std::vector<std::byte> Encode(const std::vector<std::byte>& in, int level = Z_DEFAULT_COMPRESSION);

std::vector<std::byte> Decode(const std::vector<std::byte>& in);

} // namespace zlib_wrapper

#endif // ZLIB_WRAPPER_ZLIB_WRAPPER_H
