#include "protocol.h"
#include "utils.h"
#include "log.h"
#include "client.h"

#define PROTOCOL_ERROR(_ctx, reasonfmt, ...)                   \
do {                                                           \
    sprintf_alloc(&(_ctx)->reason, reasonfmt, ## __VA_ARGS__); \
    longjmp(*(_ctx)->errlbl, 1);                               \
} while (0)

#define REMAIN_CHECK(_ctx, _req, reasonfmt, ...)                   \
do {                                                               \
    if ((size_t)(_ctx)->remain < (_req)) {                         \
        PROTOCOL_ERROR(_ctx, reasonfmt, ## __VA_ARGS__);           \
    }                                                              \
} while (0)

int8_t proto_read_sbyte_unchecked(unsigned char **buf, struct read_context *ctx) {
    --ctx->remain;
    return (int8_t)*((*buf)++);
}

uint8_t proto_read_ubyte_unchecked(unsigned char **buf, struct read_context *ctx) {
    --ctx->remain;
    return (uint8_t)*((*buf)++);
}

#define READ_BYTE_F(_type, _fname)                                                 \
_type proto_read_ ## _fname(unsigned char **buf, struct read_context *ctx) {   \
    REMAIN_CHECK(ctx, sizeof(_type), "Packet buffer depleted reading " #_type      \
                                     ": %d < %llu", (ctx)->remain, sizeof(_type)); \
    return proto_read_ ## _fname ## _unchecked(buf, ctx);                          \
}

READ_BYTE_F(int8_t, sbyte)
READ_BYTE_F(uint8_t, ubyte)

#undef READ_BYTE_F

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

void proto_handle_incoming(void *client, unsigned char *buf, struct read_context *ctx) {
    client_t *sender = client;
    if (ctx->remain == 9) {
        unsigned char wbuf[] = {14, 0, 12, '{', '"', 't', 'e', 'x', 't', '"', ':', '"', 'a', '"', '}'};
        client_write(sender, wbuf, 15);
    }

    int32_t pktid = proto_read_varint(&buf, ctx);

    packet_proc *target_func = client_protos[sender->protocol][client_proto_maxids[sender->protocol] < pktid ? 0 : pktid+1];
    if (target_func) {
        (*target_func)(client, pktid, buf, ctx);
    }
}
