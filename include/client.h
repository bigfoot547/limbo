#ifndef LIMBO_CLIENT_H_INCLUDED
#define LIMBO_CLIENT_H_INCLUDED

#include "types.h"
#include "event.h"
#include "server.h"
#include <netinet/in.h>

typedef struct tag_client {
    file_descriptor_t *fd;
    sockaddrs saddr;
    socklen_t saddrlen;
} client_t;

client_t *client_init(server_t *server, int fd, struct sockaddr *saddr, socklen_t saddrlen);
void client_free(client_t *cli);

#endif // include guard
