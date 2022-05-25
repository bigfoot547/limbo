#include "protocol.h"
#include "client.h"
#include "macros.h"
#include "utils.h"
#include "log.h"
#include "uuid.h"

#include <stdlib.h>

#define PROTOCOL_ERROR(_ctx, reasonfmt, ...)                   \
do {                                                           \
    sprintf_alloc(&(_ctx)->reason, reasonfmt, ## __VA_ARGS__); \
    longjmp(*(_ctx)->errlbl, 1);                               \
} while (0)

#define PROTO_CATCH_BEGIN(_ctx, _suf)                   \
jmp_buf _catch ## _suf, *_old ## _suf = (_ctx)->errlbl; \
(_ctx)->errlbl = &(_catch ## _suf);                     \
if (setjmp(_catch ## _suf)) {

#define PROTO_CATCH_END(_suf)     \
    longjmp(*(_old ## _suf), 1);  \
}

#define PROTO_CATCH_CLEANUP(_ctx, _suf) \
(_ctx)->errlbl = (_old ## _suf)

void proto_ignore(void *client, int32_t pktid, unsigned char *buf, struct read_context *ctx) {
    UNUSED(client); UNUSED(pktid); UNUSED(buf); UNUSED(ctx);
    ctx->remain = 0;
}

void proto_handshake_unk(void *client, int32_t pktid, unsigned char *buf, struct read_context *ctx) {
    UNUSED(client); UNUSED(buf); UNUSED(ctx);
    PROTOCOL_ERROR(ctx, "(Handshake) Received unknown packet with id %d", pktid);
}

void proto_handshake_set_protocol(void *client, int32_t pktid, unsigned char *buf, struct read_context *ctx) {
    UNUSED(pktid);

    client_t *sender = client;
    char *volatile hostname = NULL;
    int32_t len = 0;
    uint16_t port;
    unsigned nextproto;

    // read stuff
    sender->protocol_ver = proto_read_varint(&buf, ctx);

    PROTO_CATCH_BEGIN(ctx, 1)
        free(hostname);
    PROTO_CATCH_END(1)

    proto_read_lenstr(&buf, (char **)&hostname, &len, ctx);
    port = proto_read_ushort(&buf, ctx);
    nextproto = (unsigned)proto_read_varint(&buf, ctx);
    log_debug("Received handshake from %s: pvn %d, host %.*s, port %hu, nextproto: %d", sender->saddrstr, sender->protocol_ver, len, hostname, port, nextproto);

    PROTO_CATCH_CLEANUP(ctx, 1);
    free(hostname);

    // TODO: IP forwarding
    switch (nextproto) {
        case PROTOCOL_STATUS:
            sender->protocol = PROTOCOL_STATUS;
            break;
        case PROTOCOL_LOGIN:
            sender->protocol = PROTOCOL_LOGIN;
            break;
        default:
            PROTOCOL_ERROR(ctx, "(Handshake) Invalid next protocol %u - should be 1 (Status) or 2 (Login)", nextproto);
    }
}

void proto_status_request(void *client, int32_t pktid, unsigned char *buf, struct read_context *ctx) {
    UNUSED(pktid); UNUSED(buf); UNUSED(ctx);

    struct packet_status_response res;
    res.id = PKTID_WRITE_STATUS_RESPONSE;
    res.text = "{\"version\":{\"name\":\"TODO\",\"protocol\":47},\"players\":{\"max\":20,\"online\":0,\"sample\":[]},\"description\":{\"text\":\"hello world\"},\"favicon\":\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAIAAAAlC+aJAAABhGlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9TS0UqDhYRdchQnSyIijhKFYtgobQVWnUwufQLmjQkKS6OgmvBwY/FqoOLs64OroIg+AHi6OSk6CIl/i8ptIjx4Lgf7+497t4BQqPCVLNrAlA1y0jFY2I2tyoGXxHAAAQMY0Bipp5IL2bgOb7u4ePrXZRneZ/7c/QqeZMBPpF4jumGRbxBPLNp6Zz3icOsJCnE58TjBl2Q+JHrsstvnIsOCzwzbGRS88RhYrHYwXIHs5KhEk8TRxRVo3wh67LCeYuzWqmx1j35C0N5bSXNdZojiGMJCSQhQkYNZVRgIUqrRoqJFO3HPPxDjj9JLplcZTByLKAKFZLjB/+D392ahalJNykUAwIvtv0xCgR3gWbdtr+Pbbt5AvifgSut7a82gNlP0uttLXIE9G0DF9dtTd4DLneAwSddMiRH8tMUCgXg/Yy+KQf03wI9a25vrX2cPgAZ6mr5Bjg4BMaKlL3u8e7uzt7+PdPq7wdSPnKaY03N2AAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+YFFgIcA82hDYUAAAAZdEVYdENvbW1lbnQAQ3JlYXRlZCB3aXRoIEdJTVBXgQ4XAAACY0lEQVRo3u2ZTUhUURTHfzPlWDFki8SsGC2ECAqZwI1ENTCV6yhKgkiiRR9bIWjdpkXQKqJltLBFH4tMkCAigjKTVn1ROSVoi0CjQAfq3yIGfNz3xifWnXl0Dm/1f+ce7o97zzvn3peSRJItTcLNAAzAAAzAAAzAAAzgfwZYHs+tDJPwDB7AGHyDXxX+LGyGrbADOqEVMj4BUmHt9FcYhVFogQ4owQDcixFtN5yFvdDkj0AhdkvaI6WlRqlZYpHPCakkXxYKcEpKLX7e85+8NChNewAITeKfsMRj2hich+e1SuJshHM7FKAbcrACgFmYgnfwFJ7AjyBDP9yBnOccmJOKzpbYLw1X3RLT0qCUdwae858D42F7+nq8aCWpzxnrOwdeh63Tp3jLmYPTNa/EpTC3iXjRJuBCUMn7B5gNc7sN92GmaqgZuAJ3g+Ih/4XsavTXvSDdkMalOSfvR6ReKR3075Be+U/ioYWKVLd0rYLxZ+onpazj1iBddFD/vrm9UAnaY6zcBtgOwGP47rxdBmegHzb6b+bK0Li0mKvgEhzx09K5SZwJq53rYwdsgcveZh91oFnrKMdhGHphTXSoBjgKD+GYz3Y6tBfaBC+CyhYoQheMwE0Ygs/B9SnAQdgZBl8DAHcS6wBogiLsgkl4CW8qbDU4iFUHWFl1SAbaoK2eD/XNjjKVrFuJbY7yIVkAnY7yMVkArbA6qLxPFkAGuoLKl8TdzB1YKK3rHWAf9MzrDg7XLUAq4kd3Gd7CADyCPuip1LLEANjttAEYgAEYgAEYgAEYgAEYgAEYgAEYgAH8W/sNwiZofrEfFL4AAAAASUVORK5CYII=\"}";
    client_write_pkt((client_t*)client, &res);
}

void proto_status_ping(void *client, int32_t pktid, unsigned char *buf, struct read_context *ctx) {
    UNUSED(pktid);
    client_t *sender = client;

    int64_t num = proto_read_long(&buf, ctx);
    struct packet_status_pong res;
    res.id = PKTID_WRITE_STATUS_PONG;
    res.payload = num;
    client_write_pkt(sender, &res);
    sender->dc_on_write = true;
}

void proto_login_start(void *client, int32_t pktid, unsigned char *buf, struct read_context *ctx) {
    UNUSED(pktid);

    client_t *sender = client;
    char *rdname, name[17], idstr[UUID_STRLEN+1];
    int32_t namelen = 16;
    struct uuid puuid;
    memset(name, 0, 17);

    proto_read_lenstr(&buf, &rdname, &namelen, ctx);
    memcpy(name, rdname, namelen);
    free(rdname);

    uuid_gen_name(&puuid, "OfflinePlayer:", name);
    uuid_format(&puuid, idstr, UUID_STRLEN+1);
    log_info("UUID of connecting player %s: %s", name, idstr);

    player_t *player = malloc(sizeof(player_t));
    if (!player) PROTOCOL_ERROR(ctx, "Unable to allocate player_t object: malloc returned NULL");
    memset(player, 0, sizeof(player_t));
    sender->player = player;

    player->conn = sender;
    memcpy(&player->profile.id, &puuid, sizeof(struct uuid));
    memcpy(&player->profile.name, name, 17);

    struct packet_login_success res;
    res.id = PKTID_WRITE_LOGIN_SUCCESS;
    res.profile = &player->profile;
    client_write_pkt(sender, &res);
    sender->protocol = PROTOCOL_PLAY;

    // TODO: send play packets to initialize the player
}

packet_proc *const client_protos[][4 /*number of protocols*/] = {
 // Handshake
 { &proto_handshake_unk, &proto_handshake_set_protocol },

 // Status
 { &proto_ignore, &proto_status_request, &proto_status_ping },

 // Login
 { &proto_ignore, &proto_login_start },

 // Play
 { &proto_ignore }
};

const int32_t client_proto_maxids[] = { 0, 1, 0, -1 };

void proto_write_status_response(void *client, struct auto_buffer *buf, struct packet_base *pkt) {
    UNUSED(client);
    struct packet_status_response *rpkt = (struct packet_status_response *)pkt;
    proto_write_lenstr(buf, rpkt->text, -1);
}

void proto_write_status_pong(void *client, struct auto_buffer *buf, struct packet_base *pkt) {
    UNUSED(client);
    struct packet_status_pong *rpkt = (struct packet_status_pong *)pkt;
    proto_write_long(buf, rpkt->payload);
}

void proto_write_login_success(void *client, struct auto_buffer *buf, struct packet_base *pkt) {
    UNUSED(client);
    struct packet_login_success *rpkt = (struct packet_login_success *)pkt;
    char uuidstr[UUID_STRLEN + 1];

    uuid_format(&rpkt->profile->id, uuidstr, UUID_STRLEN + 1);
    proto_write_lenstr(buf, uuidstr, -1);
    proto_write_lenstr(buf, rpkt->profile->name, -1);
}

packet_write_proc *const client_write_protos[][4] = {
 { NULL },
 { &proto_write_status_response, &proto_write_status_pong },
 { NULL, NULL, &proto_write_login_success }
};
