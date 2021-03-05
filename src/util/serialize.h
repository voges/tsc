#ifndef UTIL_SERIALIZE_H
#define UTIL_SERIALIZE_H

#include <algorithm>

namespace util {

template <typename T>
std::vector<std::byte> pack(const T& in) {
    auto *begin = reinterpret_cast<const std::byte *>(&in);
    std::vector<std::byte> out(begin, begin + sizeof(T));
    return out;
}

template <typename T>
T unpack(const std::vector<std::byte>& in) {
    // assert(in.size() != sizeof(T));
    T out = 0;
    for (size_t i = 0; i < in.size(); i++) {
        out |= static_cast<T>(in[i]) << (i * 8);
    }
    return out;
}

} // namespace util

#endif // UTIL_SERIALIZE_H
