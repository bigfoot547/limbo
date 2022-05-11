#ifndef LIMBO_SERVER_H_INCLUDED
#define LIMBO_SERVER_H_INCLUDED

#include <sys/socket.h>
#include <stdbool.h>
#include "event.h"
#include "types.h"
#include "list.h"

#define SERVER_IPV6ONLY (1 << 0)

typedef struct tag_server {
    file_descriptor_t *fd;
    sockaddrs saddr;
    socklen_t saddrlen;

    dllist_t *clients;
} server_t;

// handle unix domain socket or something also?
// sockaddr_storage exists
bool server_init(const char *host, unsigned short port, unsigned flags, server_t **target);
bool server_init_unix(const char *path, unsigned flags, server_t **target);

void server_free(server_t *server);

#endif // include guard
