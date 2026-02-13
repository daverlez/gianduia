#pragma once
#include <cmath>

struct PCG32 {
    uint64_t state;
    uint64_t inc;

    PCG32(uint64_t sequence_index = 0, uint64_t delta_seq = 0) {
        seed(sequence_index, delta_seq);
    }

    void seed(uint64_t initstate, uint64_t initseq = 1) {
        state = 0u;
        inc = (initseq << 1u) | 1u;
        nextUInt();
        state += initstate;
        nextUInt();
    }

    uint32_t nextUInt() {
        uint64_t oldstate = state;
        state = oldstate * 6364136223846793005ULL + inc;
        uint32_t xorshifted = uint32_t(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint32_t rot = uint32_t(oldstate >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31));
    }

    float nextFloat() {
        return std::ldexp((float)nextUInt(), -32);
    }
};