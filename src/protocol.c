#include "protocol.h"
#include "utils.h"
#include "log.h"

#define PROTOCOL_ERROR(_lim, reasonfmt, ...)                   \
do {                                                           \
    sprintf_alloc(&(_lim)->reason, reasonfmt, ## __VA_ARGS__); \
    longjmp(*(_lim)->errlbl, 1);                               \
} while (0)

#define REMAIN_CHECK(_lim, _req, reasonfmt, ...)                   \
do {                                                               \
    if ((size_t)(_lim)->remain < (_req)) {                         \
        PROTOCOL_ERROR(_lim, reasonfmt, ## __VA_ARGS__);           \
    }                                                              \
} while (0)

int8_t proto_read_sbyte_unchecked(unsigned char **buf, struct proto_read_limit *lim) {
    --lim->remain;
    return (int8_t)*((*buf)++);
}

uint8_t proto_read_ubyte_unchecked(unsigned char **buf, struct proto_read_limit *lim) {
    --lim->remain;
    return (uint8_t)*((*buf)++);
}

#define READ_BYTE_F(_type, _fname)                                                 \
_type proto_read_ ## _fname(unsigned char **buf, struct proto_read_limit *lim) {   \
    REMAIN_CHECK(lim, sizeof(_type), "Packet buffer depleted reading " #_type      \
                                     ": %d < %llu", (lim)->remain, sizeof(_type)); \
    return proto_read_ ## _fname ## _unchecked(buf, lim);                          \
}

READ_BYTE_F(int8_t, sbyte)
READ_BYTE_F(uint8_t, ubyte)

#undef READ_BYTE_F

#define VAR_CONTINUE_FLAG (unsigned char)(0x80)
#define VAR_NUM_MASK      (unsigned char)(VAR_CONTINUE_FLAG - 1)

int32_t proto_read_varint(unsigned char **buf, struct proto_read_limit *lim) {
    int32_t ret = 0;
    uint8_t inb = 0;
    int shift = 0;

    while (shift < 32) {
        REMAIN_CHECK(lim, 1, "Packet buffer depleted reading VarInt: %d < 1", lim->remain);
        inb = proto_read_ubyte_unchecked(buf, lim);
        ret |= (uint32_t)(inb & VAR_NUM_MASK) << shift;
        if (!(inb & VAR_CONTINUE_FLAG)) return ret;
        shift += 7;
    }

    PROTOCOL_ERROR(lim, "VarInt too big");
}


int64_t proto_read_varlong(unsigned char **buf, struct proto_read_limit *lim) {
    int64_t ret = 0;
    uint8_t inb = 0;
    int shift = 0;

    while (shift < 64) {
        REMAIN_CHECK(lim, 1, "Packet buffer depleted reading VarLong: %d < 1", lim->remain);
        inb = proto_read_ubyte_unchecked(buf, lim);
        ret |= (uint64_t)(inb & VAR_NUM_MASK) << shift;
        if (!(inb & VAR_CONTINUE_FLAG)) return ret;
        shift += 7;
    }

    PROTOCOL_ERROR(lim, "VarLong too big");
}

void proto_handle_incoming(client_t *sender, unsigned char *buf, struct proto_read_limit *lim) {
    log_info("Read packet of length %d", lim->remain);
    log_info("Packet ID: %d", proto_read_varint(&buf, lim));
    while (lim->remain > 0) {
        log_info("%hhx", proto_read_ubyte(&buf, lim));
//        proto_read_ubyte(&buf, lim);
    }
}
