#include "protocol.h"
#include "utils.h"
#include "log.h"
#include "client.h"
#include "endianutils.h"

#include <string.h> // for memcpy
#include <stdlib.h>

#define PROTOCOL_ERROR(_ctx, reasonfmt, ...)                   \
do {                                                           \
    sprintf_alloc(&(_ctx)->reason, reasonfmt, ## __VA_ARGS__); \
    longjmp(*(_ctx)->errlbl, 1);                               \
} while (0)

#define REMAIN_CHECK(_ctx, _req, reasonfmt, ...)                   \
do {                                                               \
    if ((size_t)(_ctx)->remain < (size_t)(_req)) {                 \
        PROTOCOL_ERROR(_ctx, reasonfmt, ## __VA_ARGS__);           \
    }                                                              \
} while (0)

int8_t proto_read_byte_unchecked(unsigned char **buf, struct read_context *ctx) {
    --ctx->remain;
    return (int8_t)*((*buf)++);
}

uint8_t proto_read_ubyte_unchecked(unsigned char **buf, struct read_context *ctx) {
    --ctx->remain;
    return (uint8_t)*((*buf)++);
}

#define READ_BYTE_F(_type, _fname)                                                \
_type proto_read_ ## _fname(unsigned char **buf, struct read_context *ctx) {      \
    REMAIN_CHECK(ctx, sizeof(_type), "Packet buffer depleted reading " #_type     \
                                     ": %d < %lu", (ctx)->remain, sizeof(_type)); \
    return proto_read_ ## _fname ## _unchecked(buf, ctx);                         \
}

READ_BYTE_F(int8_t, byte)
READ_BYTE_F(uint8_t, ubyte)

#undef READ_BYTE_F

#define READ_INT_F2(_type, _size, _bits, _sign, _tname)                      \
_type proto_read_ ## _tname(unsigned char **buf, struct read_context *ctx) { \
    REMAIN_CHECK(ctx, _size, "Packet buffer depleted reading " #_type        \
                             ": %d < %lu", ctx->remain, _size);              \
    union { _type val; unsigned char com[_size]; } un;                       \
    memcpy(un.com, *buf, _size);                                             \
    *buf += _size;                                                           \
    ctx->remain -= _size;                                                    \
    return eu_betoh ## _sign ## _bits(un.val);                               \
}

#define READ_INT_F(_type, _bits, _sign, _tname) READ_INT_F2(_type, sizeof(_type), _bits, _sign, _tname)

READ_INT_F(int16_t, 16, s, short)
READ_INT_F(uint16_t, 16, u, ushort)

READ_INT_F(int32_t, 32, s, int)
READ_INT_F(uint32_t, 32, u, uint)

READ_INT_F(int64_t, 64, s, long)
READ_INT_F(uint64_t, 64, u, ulong)

#undef READ_INT_F
#undef READ_INT_F2

#define VAR_CONTINUE_FLAG (unsigned char)(0x80)
#define VAR_NUM_MASK      (unsigned char)(VAR_CONTINUE_FLAG - 1)

int32_t proto_read_varint(unsigned char **buf, struct read_context *ctx) {
    int32_t ret = 0;
    uint8_t inb = 0;
    int shift = 0;

    while (shift < 32) {
        REMAIN_CHECK(ctx, 1, "Packet buffer depleted reading VarInt: %d < 1", ctx->remain);
        inb = proto_read_ubyte_unchecked(buf, ctx);
        ret |= (uint32_t)(inb & VAR_NUM_MASK) << shift;
        if (!(inb & VAR_CONTINUE_FLAG)) return ret;
        shift += 7;
    }

    PROTOCOL_ERROR(ctx, "VarInt too big");
}


int64_t proto_read_varlong(unsigned char **buf, struct read_context *ctx) {
    int64_t ret = 0;
    uint8_t inb = 0;
    int shift = 0;

    while (shift < 64) {
        REMAIN_CHECK(ctx, 1, "Packet buffer depleted reading VarLong: %d < 1", ctx->remain);
        inb = proto_read_ubyte_unchecked(buf, ctx);
        ret |= (uint64_t)(inb & VAR_NUM_MASK) << shift;
        if (!(inb & VAR_CONTINUE_FLAG)) return ret;
        shift += 7;
    }

    PROTOCOL_ERROR(ctx, "VarLong too big");
}

void proto_read_bytes(unsigned char **buf, unsigned char *dest, size_t amount, struct read_context *ctx) {
    REMAIN_CHECK(ctx, amount, "Packet buffer depleted reading bytes: %d < %lu", ctx->remain, amount);
    if (dest) memcpy(dest, *buf, amount);
    *buf += amount;
    ctx->remain -= amount;
}

#define PROTO_MAX_STRLEN (32767)
void proto_read_lenstr(unsigned char **buf, char **outstr, int32_t *len, struct read_context *ctx) {
    int32_t maxlen = *len;
    if (maxlen <= 0) maxlen = PROTO_MAX_STRLEN;
    int32_t strbytes = proto_read_varint(buf, ctx);

    if (strbytes > maxlen) PROTOCOL_ERROR(ctx, "String larger than maximum length: %d > %d", strbytes, maxlen);
    if (strbytes < 0) PROTOCOL_ERROR(ctx, "String has suspicious length: %d < 0", strbytes);
    REMAIN_CHECK(ctx, strbytes, "String longer than remaining packet: %d < %d", ctx->remain, strbytes);

    char *newstr = malloc(strbytes);

    if (!newstr) PROTOCOL_ERROR(ctx, "Failed to allocate space for string (len %d): malloc returned NULL", strbytes);
    proto_read_bytes(buf, (unsigned char *)newstr, (size_t)strbytes, ctx);

    *outstr = newstr;
    *len = strbytes;
}

void proto_handle_incoming(void *client, unsigned char *buf, struct read_context *ctx) {
    client_t *sender = client;

    int32_t pktid = proto_read_varint(&buf, ctx);

    packet_proc *target_func = client_protos[sender->protocol][client_proto_maxids[sender->protocol] < pktid ? 0 : pktid+1];
    if (target_func) {
        (*target_func)(client, pktid, buf, ctx);
    }
}

int proto_write_byte(struct auto_buffer *buf, int8_t val) {
    return ab_push(buf, &val, sizeof(val));
}

int proto_write_ubyte(struct auto_buffer *buf, uint8_t val) {
    return ab_push(buf, &val, sizeof(val));
}

#define TOK_PASTE(X, Y) X ## Y
#define WRITE_INT_F(_tname, _type, _spec) \
int TOK_PASTE(proto_write_, _tname)(struct auto_buffer *buf, _type val) { \
    val = TOK_PASTE(eu_htobe, _spec)(val); \
    return ab_push2(buf, &val, 1); \
}

#define WRITE_INT_F_IPAIR(_tname, _type, _bits) \
WRITE_INT_F(_tname, _type, TOK_PASTE(s, _bits)) \
WRITE_INT_F(TOK_PASTE(u, _tname), TOK_PASTE(u, _type), TOK_PASTE(u, _bits))

WRITE_INT_F_IPAIR(short, int16_t, 16)
WRITE_INT_F_IPAIR(int, int32_t, 32)
WRITE_INT_F_IPAIR(long, int64_t, 64)

#undef WRITE_INT_F_IPAIR
#undef WRITE_INT_F
#undef TOK_PASTE

int proto_write_varint(struct auto_buffer *buf, int32_t val) {
    uint32_t uval = (uint32_t)val;
    unsigned char part;
    int res;
    do {
        part = (unsigned char)(uval & VAR_NUM_MASK);
        uval >>= 7;
        if (uval) part |= VAR_CONTINUE_FLAG;
        res = ab_push2(buf, &part, 1);
        if (res < 0) return res;
    } while (uval > 0);
    return 0;
}

int proto_write_varlong(struct auto_buffer *buf, int64_t val) {
    uint64_t uval = (uint64_t)val;
    unsigned char part;
    int res;
    do {
        part = (unsigned char)(uval & VAR_NUM_MASK);
        uval >>= 7;
        if (uval) part |= VAR_CONTINUE_FLAG;
        res = ab_push2(buf, &part, 1);
        if (res < 0) return res;
    } while (uval > 0);
    return 0;
}

int proto_write_bytes(struct auto_buffer *buf, const void *src, size_t len) {
    return ab_push(buf, src, len);
}

int proto_write_lenstr(struct auto_buffer *buf, const char *str, int32_t len) {
    int res;
    if (len < 0) {
        len = strlen(str);
    }
    res = proto_write_varint(buf, len);
    if (res == 0) res = proto_write_bytes(buf, str, (size_t)len);
    return res;
}
