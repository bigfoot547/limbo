#ifndef LIMBO_CLIENT_H_INCLUDED
#define LIMBO_CLIENT_H_INCLUDED

#include "types.h"
#include "event.h"
#include "server.h"
#include "list.h"

#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>

struct tag_client;
typedef struct tag_client client_t;

#include "protocol.h"

struct tag_client {
    file_descriptor_t *fd;
    pthread_mutex_t evtmutex;

    sockaddrs saddr;
    socklen_t saddrlen;
    char *saddrstr;

    unsigned char *recvpartial;
    size_t recvpartsz, recvpartcur, recvpartexsz;

    unsigned char *sendq;
    size_t sendqcur, sendqsz;

    protover_t protocol_ver;
    unsigned protocol;

    bool dc_on_write;
    bool should_delete;

    dllist_t *clients;
    struct list_dlnode *mypos;
};

client_t *client_init(int fd, struct sockaddr *saddr, socklen_t saddrlen);
void client_free(client_t *cli);

void client_disconnect(client_t *cli, const char *fmt, ...);
void client_write(client_t *client, const unsigned char *buf, size_t length);
void client_write_pkt(client_t *client, void *pkt);

#endif // include guard
