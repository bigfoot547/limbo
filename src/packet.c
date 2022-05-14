#include "protocol.h"
#include "client.h"
#include "macros.h"
#include "utils.h"

#define PROTOCOL_ERROR(_ctx, reasonfmt, ...)                   \
do {                                                           \
    sprintf_alloc(&(_ctx)->reason, reasonfmt, ## __VA_ARGS__); \
    longjmp(*(_ctx)->errlbl, 1);                               \
} while (0)

void proto_ignore(void *client, int32_t pktid, unsigned char *buf, struct read_context *ctx) {
    UNUSED(client); UNUSED(pktid); UNUSED(buf); UNUSED(ctx);
}

void proto_handshake_unk(void *client, int32_t pktid, unsigned char *buf, struct read_context *ctx) {
    UNUSED(client); UNUSED(buf); UNUSED(ctx);
    PROTOCOL_ERROR(ctx, "(Handshake) Received unknown packet with id %d", pktid);
}

void proto_handshake_set_protocol(void *client, int32_t pktid, unsigned char *buf, struct read_context *ctx) {
    client_t *sender = client;
    // read stuff
}

packet_proc *const client_protos[][4 /*number of protocols*/] = {
 // Handshake
 { &proto_handshake_unk, &proto_handshake_set_protocol }
};

const int32_t client_proto_maxids[] = { 0 };
