#include "client.h"
#include "log.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

void client_read_handler(file_descriptor_t *fd, void *handler_data);
void client_write_handler(file_descriptor_t *fd, void *handler_data);
void client_error_handler(file_descriptor_t *fd, int error, void *handler_data);

client_t *client_init(server_t *server, int sockfd, struct sockaddr *saddr, socklen_t saddrlen) {
    file_descriptor_t *fd = malloc(sizeof(file_descriptor_t));
    client_t *client = malloc(sizeof(client_t));

    memset(fd, 0, sizeof(file_descriptor_t));
    memset(client, 0, sizeof(client_t));

    fd->fd = sockfd;
    fd->handler_data = client;
    fd->read_handler = &client_read_handler;
    fd->write_handler = &client_write_handler;
    fd->error_handler = &client_error_handler;
    client->fd = fd;

    if (saddr->sa_family == AF_INET6) {
        struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)saddr;
        if (IN6_IS_ADDR_V4MAPPED(&saddr6->sin6_addr)) {
            client->saddrlen = sizeof(struct sockaddr_in);
            client->saddr.in.sin_family = AF_INET;
            client->saddr.in.sin_port = saddr6->sin6_port;
            memcpy(&client->saddr.in.sin_addr, saddr6->sin6_addr.s6_addr + 12, 4);
            goto addrcomplete;
        }
    }

    client->saddrlen = saddrlen;
    memcpy(&client->saddr.base, saddr, saddrlen);

addrcomplete:
    // initialize more stuff here
    return client;
}

void client_free(client_t *cli) {
    if (!cli) return;
    if (cli->fd) {
        close(cli->fd->fd);
        free(cli->fd);
    }
    free(cli);
}

void client_read_handler(file_descriptor_t *fd, void *handler_data) {
    log_info("Readable");
    char buf[4096];
    ssize_t readcnt;
    while ((readcnt = read(fd->fd, buf, 4096)) > 0) {
        log_info("Read complete");
    }

    if (readcnt == 0) {
        event_loop_delfd(fd);
        close(fd->fd); // TODO: remove from clients list
        log_info("Disconnect");
        return;
    } else if (readcnt == -1) {
        if (errno == EAGAIN) fd->state &= ~FD_CAN_READ;
        else {
            event_loop_delfd(fd);
            close(fd->fd); // TODO: remove from clients list
            log_info("Read error: %s", strerror(errno));
        }
    }
}

void client_write_handler(file_descriptor_t *fd, void *handler_data) {
    log_info("Writable");
}

void client_error_handler(file_descriptor_t *fd, int error, void *handler_data) {
    event_loop_delfd(fd);
    close(fd->fd); // TODO: remove from clients list
    log_info("Error %d", error);
}
