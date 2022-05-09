#ifndef LIMBO_CLIENT_H_INCLUDED
#define LIMBO_CLIENT_H_INCLUDED

#include "types.h"

typedef struct tag_client {
    file_descriptor_t *fd;
    sockaddrs saddr;
    socklen_t saddrlen;
} client_t;

#endif // include guard
