#ifndef LIMBO_PROTOCOL_H_INCLUDED
#define LIMBO_PROTOCOL_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include "types.h"
#include "utils.h"
#include "player.h"

struct read_context {
    int32_t remain;
    char *reason;
    jmp_buf *errlbl;
};

#define TOK_PASTE(X, Y) X ## Y

#define PROTO_READ_DEF(_tname, _type) \
_type TOK_PASTE(proto_read_, _tname)(unsigned char **buf, struct read_context *ctx)

#define PROTO_READ_DEF_IPAIR(_tname, _type) \
PROTO_READ_DEF(_tname, _type);              \
PROTO_READ_DEF(TOK_PASTE(u, _tname), TOK_PASTE(u, _type))

PROTO_READ_DEF_IPAIR(byte, int8_t);
PROTO_READ_DEF_IPAIR(short, int16_t);
PROTO_READ_DEF_IPAIR(int, int32_t);
PROTO_READ_DEF_IPAIR(long, int64_t);

PROTO_READ_DEF(varint, int32_t);
PROTO_READ_DEF(varlong, int64_t);

#undef PROTO_READ_DEF_IPAIR
#undef PROTO_READ_DEF

void proto_read_bytes(unsigned char **buf, unsigned char *dest, size_t amount, struct read_context *ctx);
void proto_read_lenstr(unsigned char **buf, char **outstr, int32_t *len, struct read_context *ctx);
// TODO: handle utf-8 instead of operating on byte strings

void proto_handle_incoming(void *client, unsigned char *buf, struct read_context *ctx);

typedef void (packet_proc)(void *, int32_t, unsigned char *, struct read_context *);

enum client_protocol {
    PROTOCOL_HANDSHAKE = 0,
    PROTOCOL_STATUS,
    PROTOCOL_LOGIN,
    PROTOCOL_PLAY
};

extern const char *const protocol_names[4];

extern packet_proc *const client_protos[][4 /*number of protocols*/];
extern const int32_t client_proto_maxids[];

#define PROTO_WRITE_DEF(_tname, _type) \
int TOK_PASTE(proto_write_, _tname)(struct auto_buffer *buf, _type val)

#define PROTO_WRITE_DEF_IPAIR(_tname, _type) \
PROTO_WRITE_DEF(_tname, _type);              \
PROTO_WRITE_DEF(TOK_PASTE(u, _tname), TOK_PASTE(u, _type))

PROTO_WRITE_DEF_IPAIR(byte, int8_t);
PROTO_WRITE_DEF_IPAIR(short, int16_t);
PROTO_WRITE_DEF_IPAIR(int, int32_t);
PROTO_WRITE_DEF_IPAIR(long, int64_t);

PROTO_WRITE_DEF(varint, int32_t);
PROTO_WRITE_DEF(varlong, int64_t);

int proto_write_bytes(struct auto_buffer *buf, const void *src, size_t len);
int proto_write_lenstr(struct auto_buffer *buf, const char *str, int32_t len);

#undef TOK_PASTE
#undef PROTO_WRITE_DEF
#undef PROTO_WRITE_DEF_IPAIR

#define PKTID_WRITE_STATUS_RESPONSE (0x00)
#define PKTID_WRITE_STATUS_PONG     (0x01)

#define PKTID_WRITE_LOGIN_SUCCESS   (0x02)

// clientbound packet definitions
#define PACKET_COMMON_FIELDS \
    int32_t id;

struct packet_base {
    PACKET_COMMON_FIELDS
};

typedef void (packet_write_proc)(void * /*client*/, struct auto_buffer * /*buffer*/, struct packet_base * /*pkt*/);
extern packet_write_proc *const client_write_protos[][4];

struct packet_status_response {
    PACKET_COMMON_FIELDS
    const char *text; // TODO: make custom struct for chat components (wchar_t involved?????)
};

struct packet_status_pong {
    PACKET_COMMON_FIELDS
    int64_t payload;
};

struct packet_login_success {
    PACKET_COMMON_FIELDS
    struct game_profile *profile;
};

#endif // include guard
