#include "client.h"
#include "log.h"
#include "utils.h"
#include "protocol.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

void client_read_handler(file_descriptor_t *fd, void *handler_data);
void client_write_handler(file_descriptor_t *fd, void *handler_data);
void client_error_handler(file_descriptor_t *fd, int error, void *handler_data);
void client_handle_complete(file_descriptor_t *fd, void *handler_data);

client_t *client_init(int sockfd, struct sockaddr *saddr, socklen_t saddrlen) {
    file_descriptor_t *fd = malloc(sizeof(file_descriptor_t));
    client_t *client = malloc(sizeof(client_t));

    memset(fd, 0, sizeof(file_descriptor_t));
    memset(client, 0, sizeof(client_t));

    fd->fd = sockfd;
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

    client->should_delete = false;
    // initialize more stuff here
    return client;
}

void client_free(client_t *cli) {
    if (!cli) return;

    if (cli->fd) {
        if (cli->fd->fd != -1) client_disconnect(cli, "Client destroyed");
        free(cli->fd);
    }
    free(cli->saddrstr);
    free(cli->recvpartial);
    free(cli);
}

void client_disconnect(client_t *cli, const char *fmt, ...) {
    va_list va;
    char *dcmsg = NULL;

    va_start(va, fmt);
    int res = vsprintf_alloc(&dcmsg, fmt, va);
    va_end(va);

    if (res < 0) {
        log_info("Disconnecting client (%s): (client_disconnect: vsprintf_alloc returned %d)", cli->saddrstr, res);
    } else {
        log_info("Disconnecting client (%s): %s", cli->saddrstr, dcmsg);
        free(dcmsg);
    }

    if (cli->fd && cli->fd->fd != -1) {
        event_loop_delfd(cli->fd);
        close(cli->fd->fd);
        cli->fd->fd = -1;
    }

    if (cli->mypos) {
        dll_removenode(cli->clients, cli->mypos);
        cli->mypos = NULL;
    }
}

#define CLIENT_READBUF_SZ (4096)

// increase to something bigger if you want to support 1.7.10
#define CLIENT_PKTLEN_MAX (32768)

void client_read_handler(file_descriptor_t *fd, void *handler_data) {
    client_t * const client = handler_data;
    log_info("Readable");
    unsigned char buf[CLIENT_READBUF_SZ];
    unsigned char *bufcur;
    ssize_t readcnt, remain;

    volatile struct proto_read_limit readlim;

    /* It's okay to cast away the volatile quantifier here because it is marked volatile
     * to prevent UB with setjmp/longjmp. If values in readlim are cached in registers, they
     * may not be restored properly when longjmp() is called. If we use the volatile struct
     * in the "exception handler" (below if statement), the register caching optimization is
     * squashed, and it doesn't matter if proto_* functions do this caching, as they aren't
     * called after setjmp(). */
    struct proto_read_limit *readlim_ptr = (struct proto_read_limit *)&readlim;
    jmp_buf exlbl;

    readlim.reason = NULL;
    readlim.errlbl = &exlbl;

    if (setjmp(exlbl)) {
        client_disconnect(client, "Protocol error: %s", readlim.reason);
        client->should_delete = true;
        free(readlim.reason);
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
                readlim.remain = (int32_t)client->recvpartexsz;
                client->recvpartexsz = client->recvpartcur = 0; // packet complete

                // cast away the volatile qualifier (it is ok because trust me)
                proto_handle_incoming(client, client->recvpartial, readlim_ptr);
            }
        }

        // the remain variable MUST be updated after a set of reads, if it is to be used again
        for (remain = readcnt - (bufcur - buf); remain > 0; remain = readcnt - (bufcur - buf)) { // there are bytes to process
            readlim.remain = remain;
            int32_t pktlen = proto_read_varint(&bufcur, readlim_ptr);
            remain = readcnt - (bufcur - buf);

            // TODO: Protocol compression
            if (pktlen <= 0) {
                client_disconnect(client, "Protocol error: Suspicious packet length: %d <= 0", pktlen);
                client->should_delete = true;
                return;
            } else if (pktlen > CLIENT_PKTLEN_MAX) {
                client_disconnect(client, "Protocol error: Packet is too long: %d > %d", pktlen, CLIENT_PKTLEN_MAX);
                client->should_delete = true;
                return;
            }

            if (pktlen > remain) {
                // oh no partial read DDDDDD:
                if ((size_t)pktlen > client->recvpartsz) {
                    client->recvpartsz = (size_t)pktlen;
                    client->recvpartial = realloc(client->recvpartial, client->recvpartsz);
                    if (!client->recvpartial) {
                        client_disconnect(client, "Protocol error: Failed to increase recvpartial length (realloc returned NULL)");
                        client->should_delete = true;
                        return;
                    }
                }

                client->recvpartexsz = pktlen;

                memcpy(client->recvpartial, bufcur, remain);
                client->recvpartcur = remain;
                break;
            } else {
                readlim.remain = pktlen;
                proto_handle_incoming(client, bufcur, readlim_ptr);
                if (readlim.remain > 0) {
                    client_disconnect(client, "Protocol error: Not all packet bytes consumed: %d > 0 (len %d)", readlim.remain, pktlen);
                    client->should_delete = true;
                    return;
                }
                bufcur += pktlen;
            }
        }
    }

    if (readcnt == 0) {
        client_disconnect(client, "Disconnected");
        client->should_delete = true;
        return;
    } else if (readcnt == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) fd->state &= ~FD_CAN_READ;
        else {
            client_disconnect(client, "Read error: %s", strerror(errno));
            client->should_delete = true;
            return;
        }
    }
}

void client_write_handler(file_descriptor_t *fd, void *handler_data) {
    log_info("Writable");
}

void client_error_handler(file_descriptor_t *fd, int error, void *handler_data) {
    client_t *client = handler_data;
    if (error == 0) client_disconnect(client, "Disconnected");
    else client_disconnect(client, "Error on socket: %s", strerror(error));
    client->should_delete = true;
}

void client_handle_complete(file_descriptor_t *fd, void *handler_data) {
    client_t *cli = handler_data;
    if (cli->should_delete) client_free(cli);
}
