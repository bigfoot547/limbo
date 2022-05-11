#include "client.h"
#include "log.h"
#include "utils.h"
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

void client_read_handler(file_descriptor_t *fd, void *handler_data) {
    client_t *client = handler_data;
    log_info("Readable");
    char buf[4096];
    ssize_t readcnt;
    while ((readcnt = read(fd->fd, buf, 4096)) > 0) {
        log_info("Read complete");
    }

    if (readcnt == 0) {
        client_disconnect(client, "Disconnected");
        client->should_delete = true;
        return;
    } else if (readcnt == -1) {
        if (errno == EAGAIN) fd->state &= ~FD_CAN_READ;
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
