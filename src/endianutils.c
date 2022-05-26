#include "endianutils.h"
#include "log.h"

#include <stdlib.h> // for abort

uint32_t eu_endian() {
    union {
        uint32_t v;
        uint8_t c[sizeof(uint32_t)];
    } test;
    test.v = (EU_ENDIAN_BIG << 24) | (EU_ENDIAN_UNSUPP << 16) | (EU_ENDIAN_UNSUPP << 8) | (EU_ENDIAN_LITTLE);
    if (test.c[0] == EU_ENDIAN_UNSUPP) {
        log_error("Unsupported endianness %02hhx (middle endian?\?\?)", test.c[0]);
        abort();
    }
    return test.c[0];
}

// below functions should be optimized into single instructions by the compiler
uint16_t eu_swapu16(uint16_t in) {
    return (in << 8) | (in >> 8);
}

uint32_t eu_swapu32(uint32_t in) {
    return (in >> 24)
        | ((in >> 8) & 0xFF00)
        | ((in << 8) & 0xFF0000)
        |  (in << 24);
}

uint64_t eu_swapu64(uint64_t in) {
    return (in >> 56)
        | ((in >> 40) & (uint64_t)0xFF00)
        | ((in >> 24) & (uint64_t)0xFF0000)
        | ((in >>  8) & (uint64_t)0xFF000000)
        | ((in <<  8) & (uint64_t)0xFF00000000)
        | ((in << 24) & (uint64_t)0xFF0000000000)
        | ((in << 40) & (uint64_t)0xFF000000000000)
        |  (in << 56);
}

// TODO: make these macros
#define SWAP_FUNC(_bits)                                          \
uint ## _bits ## _t eu_htobeu ## _bits(uint ## _bits ## _t val) { \
    switch (eu_endian()) {                                        \
        case EU_ENDIAN_LITTLE:                                    \
            return eu_swapu ## _bits(val);                        \
        case EU_ENDIAN_BIG:                                       \
        default:                                                  \
            return val;                                           \
    }                                                             \
}

#define STUB_SWAP_FUNC(_type, _fname, _rtype, _rfname) \
_type _fname(_type val) { return (_type)_rfname((_rtype)val); }

#define SWAP_FUNCS(_bits) \
SWAP_FUNC(_bits)          \
STUB_SWAP_FUNC(int ## _bits ## _t, eu_htobes ## _bits, uint ## _bits ## _t, eu_htobeu ## _bits)  \
STUB_SWAP_FUNC(uint ## _bits ## _t, eu_betohu ## _bits, uint ## _bits ## _t, eu_htobeu ## _bits) \
STUB_SWAP_FUNC(int ## _bits ## _t, eu_betohs ## _bits, uint ## _bits ## _t, eu_htobeu ## _bits)

SWAP_FUNCS(16)
SWAP_FUNCS(32)
SWAP_FUNCS(64)

#undef SWAP_FUNCS
#undef STUB_SWAP_FUNC
#undef SWAP_FUNC
