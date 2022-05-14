#ifndef LIMBO_TYPES_H_INCLUDED
#define LIMBO_TYPES_H_INCLUDED

#include <sys/un.h>
#include <netinet/in.h>
#include <stdint.h>

typedef union {
    struct sockaddr base;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
    struct sockaddr_un un;
    struct sockaddr_storage storage;
} sockaddrs;

typedef uint32_t protover_t;
#define PROTOVER_UNSET (protover_t)(-1)

#endif // include guard
