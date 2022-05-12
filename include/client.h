#ifndef LIMBO_CLIENT_H_INCLUDED
#define LIMBO_CLIENT_H_INCLUDED

#include "types.h"
#include "event.h"
#include "server.h"
#include "list.h"

#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct tag_client {
    file_descriptor_t *fd;
    sockaddrs saddr;
    socklen_t saddrlen;
    char *saddrstr;

    unsigned char *recvpartial;
    size_t recvpartsz, recvpartcur, recvpartexsz;

    bool should_delete;

    dllist_t *clients;
    struct list_dlnode *mypos;
} client_t;

client_t *client_init(int fd, struct sockaddr *saddr, socklen_t saddrlen);
void client_free(client_t *cli);

void client_disconnect(client_t *cli, const char *fmt, ...);

#endif // include guard
