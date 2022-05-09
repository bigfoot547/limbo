#ifndef LIMBO_TYPES_H_INCLUDED
#define LIMBO_TYPES_H_INCLUDED

#include <sys/un.h>
#include <netinet/in.h>

typedef union {
    struct sockaddr base;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
    struct sockaddr_un un;
    struct sockaddr_storage storage;
} sockaddrs;

#endif // include guard
