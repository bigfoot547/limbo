#ifndef LIMBO_SERVER_H_INCLUDED
#define LIMBO_SERVER_H_INCLUDED

#include <sys/socket.h>
#include <stdbool.h>
#include "event.h"

typedef struct tag_server {
    file_descriptor_t *fd;
    struct sockaddr *saddr;
    socklen_t saddrlen;
} server_t;

// handle unix domain socket or something also?
// sockaddr_storage exists
bool server_init(const char *host, unsigned short port, server_t **target);
bool server_init_unix(const char *path, server_t **target);

void server_free(server_t *server);

#endif // include guard
