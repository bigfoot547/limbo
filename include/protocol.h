#ifndef LIMBO_PROTOCOL_H_INCLUDED
#define LIMBO_PROTOCOL_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>

#include "client.h"

struct proto_read_limit {
    int32_t remain;
    char *reason;
    jmp_buf *errlbl;
};

int8_t proto_read_byte(unsigned char **buf, struct proto_read_limit *lim);
uint8_t proto_read_ubyte(unsigned char **buf, struct proto_read_limit *lim);

int32_t proto_read_varint(unsigned char **buf, struct proto_read_limit *lim);
int64_t proto_read_varlong(unsigned char **buf, struct proto_read_limit *lim);

void proto_handle_incoming(client_t *sender, unsigned char *buf, struct proto_read_limit *lim);

#endif // include guard
