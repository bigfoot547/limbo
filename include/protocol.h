#ifndef LIMBO_PROTOCOL_H_INCLUDED
#define LIMBO_PROTOCOL_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include "types.h"

struct read_context {
    int32_t remain;
    char *reason;
    jmp_buf *errlbl;
};

int8_t proto_read_byte(unsigned char **buf, struct read_context *ctx);
uint8_t proto_read_ubyte(unsigned char **buf, struct read_context *ctx);

int32_t proto_read_varint(unsigned char **buf, struct read_context *ctx);
int64_t proto_read_varlong(unsigned char **buf, struct read_context *ctx);

void proto_handle_incoming(void *client, unsigned char *buf, struct read_context *ctx);

typedef void (packet_proc)(void *, int32_t, unsigned char *, struct read_context *);

enum client_protocol {
    PROTOCOL_HANDSHAKE = 0,
    PROTOCOL_STATUS,
    PROTOCOL_LOGIN,
    PROTOCOL_PLAY
};

extern packet_proc *const client_protos[][4 /*number of protocols*/];
extern const int32_t client_proto_maxids[];

#endif // include guard
