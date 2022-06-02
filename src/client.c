#include "client.h"
#include "log.h"
#include "utils.h"
#include "protocol.h"
#include "macros.h"
#include "sched.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <time.h>

void client_disconnect_internal(client_t *client, const char *fmt, ...);

void client_read_handler(file_descriptor_t *fd, void *handler_data);
void client_write_handler(file_descriptor_t *fd, void *handler_data);
void client_error_handler(file_descriptor_t *fd, int error, void *handler_data);
void client_handle_complete(file_descriptor_t *fd, void *handler_data);

client_t *client_init(int sockfd, struct sockaddr *saddr, socklen_t saddrlen) {
    file_descriptor_t *fd = malloc(sizeof(file_descriptor_t));
    client_t *client = malloc(sizeof(client_t));

    memset(fd, 0, sizeof(file_descriptor_t));
    memset(client, 0, sizeof(client_t));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&client->evtmutex, &attr);

    fd->fd = sockfd;
    fd->handler_mutex = &client->evtmutex;
    fd->handler_data = client;
    fd->read_handler = &client_read_handler;
    fd->write_handler = &client_write_handler;
    fd->error_handler = &client_error_handler;
    fd->handle_complete = &client_handle_complete;
    client->fd = fd;

    if (saddr->sa_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)saddr)->sin6_addr)) {
        struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)saddr;
        client->saddrlen = sizeof(struct sockaddr_in);
        client->saddr.in.sin_family = AF_INET;
        client->saddr.in.sin_port = saddr6->sin6_port;
        memcpy(&client->saddr.in.sin_addr, saddr6->sin6_addr.s6_addr + 12, 4);
    } else {
        client->saddrlen = saddrlen;
        memcpy(&client->saddr.base, saddr, saddrlen);
    }

    char addrbuf[128];
    switch (client->saddr.base.sa_family) {
        case AF_INET:
            sprintf_alloc(&client->saddrstr, "%s:%hu",
                inet_ntop(AF_INET, &client->saddr.in.sin_addr, addrbuf, 128),
                ntohs(client->saddr.in.sin_port));
            break;
        case AF_INET6:
            sprintf_alloc(&client->saddrstr, "[%s]:%hu",
                inet_ntop(AF_INET6, &client->saddr.in6.sin6_addr, addrbuf, 128),
                ntohs(client->saddr.in6.sin6_port));
            break;
        case AF_UNIX: {
            const char *host = "unix-local";
            size_t len = strlen(host) + 1;
            client->saddrstr = malloc(len);
            memcpy(client->saddrstr, host, len);
            break;
        }
        default: {
            const char *host = "unknown";
            size_t len = strlen(host) + 1;
            client->saddrstr = malloc(len);
            memcpy(client->saddrstr, host, len);
        }
    }

    client->sendq = NULL;

    client->protocol_ver = PROTOVER_UNSET;
    client->protocol = PROTOCOL_HANDSHAKE;

    if (sched_timer_wgettime(CLOCK_MONOTONIC, &client->lastping) < 0) {
        log_error("client_init(%s): sched_timer_wgettime failed when setting lastping: %s", strerror(errno));
    }
    client->pingrespond = true;
    client->latency_ms = -1;

    // initialize more stuff here
    return client;
}

void client_free(client_t *cli) {
    if (!cli) return;

    if (cli->fd) {
        if (cli->fd->fd != -1) client_disconnect_internal(cli, "Client destroyed");
        free(cli->fd);
    }

    if (cli->mypos) {
        dll_removenode(cli->clients, cli->mypos);
        cli->mypos = NULL;
    }

    pthread_mutex_destroy(&cli->evtmutex);
    free(cli->saddrstr);
    free(cli->recvpartial);
    free(cli->sendq);
    free(cli->textures);
    free(cli->texsig);
    free(cli->player);
    free(cli);
}

void client_actually_disconnect_for_real(client_t *cli) {
    pthread_mutex_lock(&cli->evtmutex);
    if (cli->fd && cli->fd->fd != -1) {
        event_loop_delfd(cli->fd);
        close(cli->fd->fd);
        cli->fd->fd = -1;
    }
    pthread_mutex_unlock(&cli->evtmutex);
}

void client_disconnect_vw(client_t *cli, const wchar_t *fmt, va_list va) {
    if (fmt) {
        wchar_t *dcmsg = NULL;

        int res = vswprintf_alloc(&dcmsg, fmt, va);

        unsigned level = (cli->protocol > PROTOCOL_STATUS) ? LOG_INFO : LOG_DEBUG;
        if (res < 0) {
            log_log(level, "Disconnecting client (%s) (%s): (client_disconnect_vw: vsprintf_alloc returned %d)", cli->saddrstr, protocol_names[cli->protocol], res);
        } else {
            log_log(level, "Disconnecting client (%s) (%s): %ls", cli->saddrstr, protocol_names[cli->protocol], dcmsg);
            free(dcmsg);
        }
    }

    client_actually_disconnect_for_real(cli);
}

void client_disconnect_v(client_t *cli, const char *fmt, va_list va) {
    if (fmt) {
        char *dcmsg = NULL;

        int res = vsprintf_alloc(&dcmsg, fmt, va);

        unsigned level = (cli->protocol > PROTOCOL_STATUS) ? LOG_INFO : LOG_DEBUG;
        if (res < 0) {
            log_log(level, "Disconnecting client (%s) (%s): (client_disconnect_v: vsprintf_alloc returned %d)", cli->saddrstr, protocol_names[cli->protocol], res);
        } else {
            log_log(level, "Disconnecting client (%s) (%s): %s", cli->saddrstr, protocol_names[cli->protocol], dcmsg);
            free(dcmsg);
        }
    }

    client_actually_disconnect_for_real(cli);
}

void client_disconnect(client_t *cli, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    client_disconnect_v(cli, fmt, va);
    va_end(va);
}

void client_disconnect_w(client_t *cli, const wchar_t *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    client_disconnect_vw(cli, fmt, va);
    va_end(va);
}

void client_kick_w(client_t *cli, const wchar_t *fmt, ...) {
    wchar_t *reason = NULL, *reason_com = NULL;
    va_list va;

    va_start(va, fmt);
    int res = vswprintf_alloc(&reason, fmt, va);
    va_end(va);

    if (res >= 0 && reason) {
        res = swprintf_alloc(&reason_com, L"{\"text\":\"%ls\"}", reason);
    }

    if (res >= 0 && reason_com) {
        unsigned proto = cli->protocol;

        switch(proto) {
            case PROTOCOL_HANDSHAKE:
            case PROTOCOL_LOGIN: {
                struct packet_login_disconnect pkt = {
                    .id = PKTID_WRITE_LOGIN_DISCONNECT,
                    .wide = true,
                    .text = { .w = reason }
                };

                /* The client's protocol is temporarily changed because PROTOCOL_HANDSHAKE has no
                 * write methods set. This is "safe" to do because the client is probably already
                 * in the PROTOCOL_LOGIN state by the time we receive their handshake Set Protocol
                 * packet.
                 */
                cli->protocol = PROTOCOL_LOGIN;
                client_write_pkt(cli, &pkt);
                cli->protocol = proto;
                break;
            }
            case PROTOCOL_PLAY: {
                struct packet_play_disconnect pkt = {
                    .id = PKTID_WRITE_PLAY_DISCONNECT,
                    .wide = true,
                    .text = { .w = reason }
                };

                client_write_pkt(cli, &pkt);
                break;
            }
        }
    }
    free(reason_com);

    client_disconnect_w(cli, L"Kicked: %ls", reason);
    free(reason);
}

void client_disconnect_internal(client_t *cli, const char *fmt, ...) {
    va_list va;

    pthread_mutex_lock(&cli->evtmutex);

    va_start(va, fmt);
    client_disconnect_v(cli, fmt, va);
    va_end(va);

    if (cli->fd) { // mark the client to be freed
        cli->fd->state |= FD_CALL_COMPLETE;
    }

    if (cli->mypos) { // remove the client from the clients list
        dll_removenode(cli->clients, cli->mypos);
        cli->mypos = NULL;
    }

    pthread_mutex_unlock(&cli->evtmutex);
}

#define CLIENT_READBUF_SZ (4096)

// may need to be bigger
#define CLIENT_PKTLEN_MAX (65536)

void client_read_handler(file_descriptor_t *fd, void *handler_data) {
    client_t *const client = handler_data;
    unsigned char buf[CLIENT_READBUF_SZ];
    unsigned char *bufcur;
    ssize_t readcnt, remain;

    volatile struct read_context readctx;

    /* It's okay to cast away the volatile quantifier here because it is marked volatile
     * to prevent UB with setjmp/longjmp. If values in readctx are cached in registers, they
     * may not be restored properly when longjmp() is called. If we use the volatile struct
     * in the "exception handler" (below if statement), the register caching optimization is
     * squashed, and it doesn't matter if proto_* functions do this caching, as they aren't
     * called after setjmp(). */
    struct read_context *readctx_ptr = (struct read_context *)&readctx;
    jmp_buf exlbl, dclbl;

    readctx.protover = client->protocol_ver;
    readctx.reason = NULL;
    readctx.errlbl = &exlbl;
    readctx.dclbl = &dclbl;

    if (setjmp(exlbl)) {
        if (readctx.reason)
            client_disconnect_internal(client, "Protocol error: %s", readctx.reason);
        else
            client_disconnect_internal(client, NULL);
        free(readctx.reason);
        return;
    }

    if (setjmp(dclbl)) {
        if (readctx.reasonw)
            client_kick_w(client, L"%ls", readctx.reasonw);
        free(readctx.reasonw);
        fd->state |= FD_CALL_COMPLETE;
        return;
    }

    while ((readcnt = read(fd->fd, buf, CLIENT_READBUF_SZ)) > 0) {
        bufcur = buf;
        if (client->recvpartexsz > 0) { // we are expecting the rest of a packet
            ssize_t partremain = client->recvpartexsz - client->recvpartcur;
            if (partremain > readcnt) { // this packet is expecting more than could be read
                memcpy(client->recvpartial + client->recvpartcur, buf, readcnt);
                client->recvpartcur += readcnt;
                continue; // we have already consumed the entire read buffer
            } else {
                memcpy(client->recvpartial + client->recvpartcur, buf, partremain);
                bufcur += partremain;
                readctx.remain = (int32_t)client->recvpartexsz;
                client->recvpartexsz = client->recvpartcur = 0; // packet complete

                // cast away the volatile qualifier (it is ok because trust me)
                proto_handle_incoming(client, client->recvpartial, readctx_ptr);
            }
        }

        // the remain variable MUST be updated after a set of reads, if it is to be used again
        for (remain = readcnt - (bufcur - buf); remain > 0; remain = readcnt - (bufcur - buf)) { // there are bytes to process
            readctx.remain = remain;
            // check for FE 01 FA (legacy ping) here
            int32_t pktlen = proto_read_varint(&bufcur, readctx_ptr);
            remain = readcnt - (bufcur - buf);

            // TODO: Protocol compression
            if (pktlen <= 0) {
                client_disconnect_internal(client, "Protocol error: Suspicious packet length: %d <= 0", pktlen);
                return;
            } else if (pktlen > CLIENT_PKTLEN_MAX) {
                client_disconnect_internal(client, "Protocol error: Packet is too long: %d > %d", pktlen, CLIENT_PKTLEN_MAX);
                return;
            }

            if (pktlen > remain) {
                // oh no partial read DDDDDD:
                if ((size_t)pktlen > client->recvpartsz) {
                    log_debug("Resizing client recvq (%s): from %lu to %d", client->saddrstr, client->recvpartsz, pktlen);
                    client->recvpartsz = (size_t)pktlen;
                    void *newalloc = realloc(client->recvpartial, client->recvpartsz);
                    if (!newalloc) {
                        client_disconnect_internal(client, "Protocol error: Failed to increase recvpartial length (realloc returned NULL)");
                        return;
                    }
                    client->recvpartial = newalloc;
                }

                client->recvpartexsz = pktlen;

                memcpy(client->recvpartial, bufcur, remain);
                client->recvpartcur = remain;
                break;
            } else {
                readctx.remain = pktlen;
                proto_handle_incoming(client, bufcur, readctx_ptr);
                if (readctx.remain > 0) {
                    client_disconnect_internal(client, "Protocol error: Not all packet bytes consumed: %d > 0 (len %d)", readctx.remain, pktlen);
                    return;
                }
                bufcur += pktlen;
            }
        }
    }

    if (readcnt == 0) {
        client_disconnect_internal(client, "Disconnected");
        return;
    } else if (readcnt == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) fd->state &= ~FD_CAN_READ;
        else {
            client_disconnect_internal(client, "Read error: %s", strerror(errno));
            return;
        }
    }
}

// 1 MB max SendQ
#define CLIENT_MAX_SENDQ (1000000ul)

void client_add_sendq(client_t *client, const unsigned char *buf, size_t length) {
    if (client->sendqcur + length > client->sendqsz) {
        log_debug("Resizing client sendq (%s): from %lu to %lu", client->saddrstr, client->sendqsz, client->sendqcur + length);
        client->sendqsz = client->sendqcur + length;

        if (client->sendqsz > CLIENT_MAX_SENDQ) {
            client_disconnect_internal(client, "Max send queue exceeded (%lu > %lu)", client->sendqsz, CLIENT_MAX_SENDQ);
            return;
        }

        void *newsendq = realloc(client->sendq, client->sendqsz);
        if (!newsendq) {
            client_disconnect_internal(client, "Protocol error: Failed to increase sendq length (realloc returned NULL)");
            return;
        }
        client->sendq = newsendq;
    }
    memcpy(client->sendq + client->sendqcur, buf, length);
    client->sendqcur += length;
}

void client_write(client_t *client, const unsigned char *buf, size_t length) {
    pthread_mutex_lock(&client->evtmutex);
    if (client->fd->state & FD_CAN_WRITE) {
        ssize_t writecnt;
        while (length > 0 && (writecnt = write(client->fd->fd, buf, length)) > 0) {
            buf += writecnt;
            length -= writecnt;
            if (writecnt == 0) log_debug("client_write: write returned 0");
        }

        if (writecnt < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                client->fd->state &= ~FD_CAN_WRITE;
                // make sure the event loop knows we want a write again
                // TODO: test for race conditions
                event_loop_want(client->fd, client->fd->state);
                if (length > 0) client_add_sendq(client, buf, length);
            } else {
                client_disconnect_internal(client, "Write error: %s", strerror(errno));
                goto writedone;
            }
        }
    } else {
        client_add_sendq(client, buf, length);
    }

    if (client->dc_on_write && client->sendqcur == 0) {
        client_disconnect_internal(client, NULL);
        log_debug("Disconnecting client %s, dc_on_write was set.", client->saddrstr);
    }

writedone:
    pthread_mutex_unlock(&client->evtmutex);
}

void client_write_pkt(client_t *client, void *pkt) {
    struct packet_base *bpkt = pkt;
    packet_write_proc *proc = client_write_protos[client->protocol][bpkt->id];
    if (!proc) {
        log_error("BUG: Attempted to write a %s packet to %s with unsupported ID %d!", protocol_names[client->protocol], client->saddrstr, bpkt->id);
#ifdef BUILD_DEBUG
        abort();
#endif
        return;
    }

    struct auto_buffer pkt_writebuf, pkt_writebuf2;
    ab_init(&pkt_writebuf, 0, 0);
    proto_write_varint(&pkt_writebuf, bpkt->id);
    (*proc)(client, &pkt_writebuf, bpkt);

    size_t pktlen = ab_getwrcur(&pkt_writebuf);
    ab_init(&pkt_writebuf2, pktlen + 7, 0);
    proto_write_varint(&pkt_writebuf2, (int32_t)pktlen);
    ab_push2(&pkt_writebuf2, pkt_writebuf.buf, pktlen);
    client_write(client, pkt_writebuf2.buf, ab_getwrcur(&pkt_writebuf2));

    for (size_t i = 0, max = ab_getwrcur(&pkt_writebuf2); i < max; ++i) {
        printf("%2.2hhx ", pkt_writebuf2.buf[i]);
    }
    putchar('\n');

    ab_free(&pkt_writebuf2);
    ab_free(&pkt_writebuf);
}

void client_write_handler(file_descriptor_t *fd, void *handler_data) {
    client_t *client = handler_data;

    // flush sendq (or as much of it as we can)
    ssize_t writecnt = 0;
    unsigned char *writecur = client->sendq;
    while (client->sendqcur > 0 && (writecnt = write(fd->fd, writecur, client->sendqcur)) > 0) {
        writecur += writecnt;
        client->sendqcur -= writecnt;
        if (writecnt == 0) log_debug("client_write_handler: write returned 0"); // how does this happen?
    }

    if (writecnt < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (client->sendqcur == 0) {
                if (client->dc_on_write) {
                    log_debug("Disconnecting client %s, dc_on_write was set.", client->saddrstr);
                    client_disconnect_internal(client, NULL);
                }
                return;
            }
            memmove(client->sendq, writecur, client->sendqsz - (writecur - client->sendq));
        } else {
            client_disconnect_internal(client, "Write error: %s", strerror(errno));
            return;
        }
    }
}

void client_error_handler(file_descriptor_t *fd, int error, void *handler_data) {
    client_t *client = handler_data;
    UNUSED(fd);

    if (error == 0) client_disconnect_internal(client, "Disconnected");
    else client_disconnect_internal(client, "Error on socket: %s", strerror(error));
}

void client_handle_complete(file_descriptor_t *fd, void *handler_data) {
    client_t *cli = handler_data;
    UNUSED(fd);

    client_free(cli);
}
