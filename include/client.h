#ifndef LIMBO_CLIENT_H_INCLUDED
#define LIMBO_CLIENT_H_INCLUDED

typedef struct tag_client {
    file_descriptor_t *fd;
    struct sockaddr *saddr;
    socklen_t saddrlen;

    // TODO
} client_t;

#endif // include guard
