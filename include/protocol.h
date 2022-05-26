#ifndef LIMBO_PROTOCOL_H_INCLUDED
#define LIMBO_PROTOCOL_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>

#include "types.h"
#include "utils.h"
#include "player.h"
#include "world.h"

struct read_context {
    int32_t remain;
    int32_t protover;
    char *reason;
    jmp_buf *errlbl;
};

#define TOK_PASTE(X, Y) X ## Y

#define PROTO_READ_DEF(_tname, _type) \
_type TOK_PASTE(proto_read_, _tname)(unsigned char **buf, struct read_context *ctx)

#define PROTO_READ_DEF_IPAIR(_tname, _type) \
PROTO_READ_DEF(_tname, _type);              \
PROTO_READ_DEF(TOK_PASTE(u, _tname), TOK_PASTE(u, _type))

bool proto_read_bool(unsigned char **buf, struct read_context *ctx);

PROTO_READ_DEF_IPAIR(byte, int8_t);
PROTO_READ_DEF_IPAIR(short, int16_t);
PROTO_READ_DEF_IPAIR(int, int32_t);
PROTO_READ_DEF_IPAIR(long, int64_t);

PROTO_READ_DEF(varint, int32_t);
PROTO_READ_DEF(varlong, int64_t);

PROTO_READ_DEF(float, float);
PROTO_READ_DEF(double, double);

#undef PROTO_READ_DEF_IPAIR
#undef PROTO_READ_DEF

void proto_read_bytes(unsigned char **buf, unsigned char *dest, size_t amount, struct read_context *ctx);
void proto_read_lenstr(unsigned char **buf, char **outstr, int32_t *len, struct read_context *ctx);
// TODO: handle utf-8 instead of operating on byte strings

void proto_read_blockpos(unsigned char **buf, struct block_position *pos, struct read_context *ctx);

void proto_handle_incoming(void *client, unsigned char *buf, struct read_context *ctx);

typedef void (packet_proc)(void *, int32_t, unsigned char *, struct read_context *);

enum client_protocol {
    PROTOCOL_HANDSHAKE = 0,
    PROTOCOL_STATUS,
    PROTOCOL_LOGIN,
    PROTOCOL_PLAY
};

#define PROTOCOL_COUNT (4)

extern const char *const protocol_names[PROTOCOL_COUNT];

extern packet_proc *const *client_protos[PROTOCOL_COUNT];
extern const int32_t client_proto_maxids[PROTOCOL_COUNT];

#define PROTO_WRITE_DEF(_tname, _type) \
int TOK_PASTE(proto_write_, _tname)(struct auto_buffer *buf, _type val)

#define PROTO_WRITE_DEF_IPAIR(_tname, _type) \
PROTO_WRITE_DEF(_tname, _type);              \
PROTO_WRITE_DEF(TOK_PASTE(u, _tname), TOK_PASTE(u, _type))

int proto_write_bool(struct auto_buffer *buf, bool val);

PROTO_WRITE_DEF_IPAIR(byte, int8_t);
PROTO_WRITE_DEF_IPAIR(short, int16_t);
PROTO_WRITE_DEF_IPAIR(int, int32_t);
PROTO_WRITE_DEF_IPAIR(long, int64_t);

PROTO_WRITE_DEF(varint, int32_t);
PROTO_WRITE_DEF(varlong, int64_t);

PROTO_WRITE_DEF(float, float);
PROTO_WRITE_DEF(double, double);

int proto_write_bytes(struct auto_buffer *buf, const void *src, size_t len);
int proto_write_lenstr(struct auto_buffer *buf, const char *str, int32_t len);

PROTO_WRITE_DEF(blockpos, const struct block_position *);

#undef TOK_PASTE
#undef PROTO_WRITE_DEF
#undef PROTO_WRITE_DEF_IPAIR

#define PKTID_WRITE_STATUS_RESPONSE (0x00)
#define PKTID_WRITE_STATUS_PONG     (0x01)

#define PKTID_WRITE_LOGIN_SUCCESS   (0x02)

#define PKTID_WRITE_PLAY_KEEP_ALIVE (0x00)
#define PKTID_WRITE_PLAY_JOIN_GAME  (0x01)
#define PKTID_WRITE_PLAY_SPAWN_POS  (0x05)
#define PKTID_WRITE_PLAY_PLAYER_POS_LOOK (0x08)

// clientbound packet definitions
#define PACKET_COMMON_FIELDS \
    int32_t id;

struct packet_base {
    PACKET_COMMON_FIELDS
};

typedef void (packet_write_proc)(void * /*client*/, struct auto_buffer * /*buffer*/, struct packet_base * /*pkt*/);
extern packet_write_proc *const *client_write_protos[PROTOCOL_COUNT];

// Clientbound Status packets

struct packet_status_response {
    PACKET_COMMON_FIELDS
    const char *text; // TODO: make custom struct for chat components (wchar_t involved?????)
};

struct packet_status_pong {
    PACKET_COMMON_FIELDS
    int64_t payload;
};

// Clientbound Login packets

struct packet_login_success {
    PACKET_COMMON_FIELDS
    struct game_profile *profile;
};

// Clientbound Play packets

struct packet_play_keep_alive {
    PACKET_COMMON_FIELDS
    int32_t payload;
};

struct packet_play_join_game {
    PACKET_COMMON_FIELDS
    int32_t peid;
    uint8_t gamemode;
    int8_t dimension;
    uint8_t difficulty;
    uint8_t max_players;
    const char *level_type;
    bool reduced_dbg_info;
};

struct packet_play_spawn_position {
    PACKET_COMMON_FIELDS
    struct block_position pos;
};

enum player_pos_look_flags {
    PPL_X_REL = 0x01,
    PPL_Y_REL = 0x02,
    PPL_Z_REL = 0x04,
    PPL_Y_ROT_REL = 0x08,
    PPL_X_ROT_REL = 0x10
};

struct packet_play_player_position_look {
    PACKET_COMMON_FIELDS
    double x, y, z;
    float yaw, pitch;
    uint8_t flags;
};

#endif // include guard
