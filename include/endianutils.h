#ifndef LIMBO_ENDIANUTILS_H_INCLUDED
#define LIMBO_ENDIANUTILS_H_INCLUDED

#include <stdint.h>

#define EU_ENDIAN_LITTLE (0x00)
#define EU_ENDIAN_BIG    (0x01)
#define EU_ENDIAN_UNSUPP (0x02)

uint32_t eu_endian();

uint16_t eu_swapu16(uint16_t val);
uint32_t eu_swapu32(uint32_t val);
uint64_t eu_swapu64(uint64_t val);

#define TOK_PASTE(X, Y) X ## Y
#define TOK_PASTE3(X, Y, Z) X ## Y ## Z

#define EU_SWAPDEF(_tname, _type)             \
_type TOK_PASTE(eu_htobe, _tname)(_type val); \
_type TOK_PASTE(eu_betoh, _tname)(_type val)

#define EU_SWAPDEF_IPAIR(_bits)                              \
EU_SWAPDEF(TOK_PASTE(s, _bits), TOK_PASTE3(int, _bits, _t)); \
EU_SWAPDEF(TOK_PASTE(u, _bits), TOK_PASTE3(uint, _bits, _t))

EU_SWAPDEF_IPAIR(16);
EU_SWAPDEF_IPAIR(32);
EU_SWAPDEF_IPAIR(64);

#undef EU_SWAPDEF_IPAIR
#undef EU_SWAPDEF
#undef TOK_PASTE3
#undef TOK_PASTE

#endif // include guard
