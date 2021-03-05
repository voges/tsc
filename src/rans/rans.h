#ifndef RANS_RANS_H
#define RANS_RANS_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <iostream>
#include <vector>

#include "rans_byte.h"

struct SymbolStats
{
    SymbolStats(const std::vector<uint32_t>& f) {
        for (int i = 0; i < 256; i++) {
            freqs[i] = f[i];
        }
        CalcCumFreqs();
    }

    uint32_t freqs[256];
    uint32_t cum_freqs[257];

    void CalcCumFreqs();
    void NormalizeFreqs(uint32_t target_total);
};

void SymbolStats::CalcCumFreqs()
{
    cum_freqs[0] = 0;
    for (int i=0; i < 256; i++)
        cum_freqs[i+1] = cum_freqs[i] + freqs[i];
}

void SymbolStats::NormalizeFreqs(uint32_t target_total)
{
    assert(target_total >= 256);

    CalcCumFreqs();
    uint32_t cur_total = cum_freqs[256];

    // resample distribution based on cumulative freqs
    for (int i = 1; i <= 256; i++)
        cum_freqs[i] = ((uint64_t)target_total * cum_freqs[i])/cur_total;

    // if we nuked any non-0 frequency symbol to 0, we need to steal
    // the range to make the frequency nonzero from elsewhere.
    //
    // this is not at all optimal, i'm just doing the first thing that comes to mind.
    for (int i=0; i < 256; i++) {
        if (freqs[i] && cum_freqs[i+1] == cum_freqs[i]) {
            // symbol i was set to zero freq

            // find best symbol to steal frequency from (try to steal from low-freq ones)
            uint32_t best_freq = ~0u;
            int best_steal = -1;
            for (int j=0; j < 256; j++) {
                uint32_t freq = cum_freqs[j+1] - cum_freqs[j];
                if (freq > 1 && freq < best_freq) {
                    best_freq = freq;
                    best_steal = j;
                }
            }
            assert(best_steal != -1);

            // and steal from it!
            if (best_steal < i) {
                for (int j = best_steal + 1; j <= i; j++)
                    cum_freqs[j]--;
            } else {
                assert(best_steal > i);
                for (int j = i + 1; j <= best_steal; j++)
                    cum_freqs[j]++;
            }
        }
    }

    // calculate updated freqs and make sure we didn't screw anything up
    assert(cum_freqs[0] == 0 && cum_freqs[256] == target_total);
    for (int i=0; i < 256; i++) {
        if (freqs[i] == 0)
            assert(cum_freqs[i+1] == cum_freqs[i]);
        else
            assert(cum_freqs[i+1] > cum_freqs[i]);

        // calc updated freq
        freqs[i] = cum_freqs[i+1] - cum_freqs[i];
    }

    // debug print
    // std::cout << "freqs:" << std::endl;
    // for (int i = 0; i < 256; i++) {
    //     std::cout << freqs[i] << " ";
    // }
    // std::cout << std::endl;
}


#include <time.h>

namespace rans {

const uint32_t prob_bits = 14;
const uint32_t prob_scale = 1 << prob_bits;
// freqs are in [0,(2^prob_bits)] (not in [0,(2^prob_bits)-1]!)

static std::vector<uint32_t> CountFreqs(std::vector<std::byte> in) {
    std::vector<uint32_t> freqs(256, 0);
    for (const auto& b: in) {
        freqs[std::to_integer<int>(b)]++;
    }
    return freqs;
}

std::vector<std::byte> Encode(const std::vector<std::byte>& in) {
    assert(in.size() != 0);

    auto freqs = rans::CountFreqs(in);
    SymbolStats stats(freqs);
    stats.NormalizeFreqs(rans::prob_scale);

    static size_t out_max_size = 32<<20; // 32MB
    // add assertion, assume worst case compression
    uint8_t* tmp = new uint8_t[out_max_size];

    RansEncSymbol esyms[256];
    for (int i=0; i < 256; i++) {
        RansEncSymbolInit(&esyms[i], stats.cum_freqs[i], stats.freqs[i], rans::prob_bits);
    }

    RansState rans;
    RansEncInit(&rans);

    uint8_t* ptr = tmp + out_max_size; // *end* of output buffer
    uint8_t *in_bytes = reinterpret_cast<uint8_t*>(const_cast<std::byte*>(in.data()));
    for (size_t i=in.size(); i > 0; i--) { // NB: working in reverse!
        int s = in_bytes[i-1];
        RansEncPutSymbol(&rans, &ptr, &esyms[s]);
    }
    RansEncFlush(&rans, &ptr);

    const size_t out_size = tmp + out_max_size - ptr;
    std::vector<std::byte> out(out_size);
    std::transform(ptr, (ptr + out_size), out.begin(), [] (uint8_t b) { return std::byte(b); });
    delete[] tmp;

    return out;
}

std::vector<std::byte> Decode(
        std::vector<uint32_t> freqs,
        const std::vector<std::byte>& compressed,
        const size_t uncompressed_size
) {
    SymbolStats stats(freqs);
    // should already have normalized freqs here
    stats.NormalizeFreqs(rans::prob_scale);

    uint8_t* tmp = new uint8_t[uncompressed_size];

    RansDecSymbol dsyms[256];
    for (int i=0; i < 256; i++) {
        RansDecSymbolInit(&dsyms[i], stats.cum_freqs[i], stats.freqs[i]);
    }

    // cumulative->symbol table
    uint8_t cum2sym[rans::prob_scale];
    for (int s=0; s < 256; s++)
        for (uint32_t i=stats.cum_freqs[s]; i < stats.cum_freqs[s+1]; i++)
            cum2sym[i] = s;

    RansState rans;
    uint8_t *ptr = reinterpret_cast<uint8_t*>(const_cast<std::byte *>(compressed.data()));
    RansDecInit(&rans, &ptr);

    for (size_t i=0; i < uncompressed_size; i++) {
        uint32_t s = cum2sym[RansDecGet(&rans, rans::prob_bits)];
        tmp[i] = (uint8_t) s;
        RansDecAdvanceSymbol(&rans, &ptr, &dsyms[s], rans::prob_bits);
    }

    std::vector<std::byte> out(uncompressed_size);
    std::transform(tmp, (tmp + uncompressed_size), out.begin(), [] (uint8_t b) { return std::byte(b); });
    delete[] tmp;
    return out;
}

} // namespace rans

#endif // RANS_RANS_H
