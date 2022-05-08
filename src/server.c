#include "server.h"

// hello this is bigfoot and here we like to optimize compile time
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "log.h"

void server_handle_read(file_descriptor_t *fd, void *handler_info);

int make_fd_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) return flags;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool server_init(const char *host, unsigned short port, server_t **target) {
    struct addrinfo *res = NULL, hints;
    memset(&hints, 0, sizeof(hints));
    bool success = true;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE; // we intend to use bind(2) on this sockaddr

    char *portnum = NULL;
    int len = snprintf(portnum, 0, "%hu", port);
    portnum = malloc(len + 1);
    snprintf(portnum, len + 1, "%hu", port);

    int result = getaddrinfo(host, portnum, &hints, &res);
    free(portnum);

    if (result != 0) {
        log_error("server_init unable to lookup \"%s\" port %hu: getaddrinfo: %s", host, port, gai_strerror(result));
        success = false;
        goto done;
    }

    struct addrinfo *cursor = res;
    while (cursor) {
        // ignore weird address families
        if (cursor->ai_family != AF_INET && cursor->ai_family != AF_INET6) continue;

        // try binding to hosts until it works
        int sockfd = socket(cursor->ai_family, SOCK_STREAM, 0 /*or IPPROTO_TCP*/);
        if (sockfd < 0) {
            log_error("server_init(%s, %hu): socket: %s", host, port, strerror(errno));
            success = false;
            goto done;
        }

        if (make_fd_nonblock(sockfd) < 0) {
            log_error("server_init(%s, %hu): fcntl: %s", host, port, strerror(errno));
            close(sockfd);
            success = false;
            goto done;
        }

        if (bind(sockfd, cursor->ai_addr, cursor->ai_addrlen) < 0) {
            log_error("server_init(%s, %hu): bind: %s", host, port, strerror(errno));
//            close(sockfd);
//            cursor = cursor->ai_next;
//            continue;
            goto failcontinue;
        }

        if (listen(sockfd, 20) < 0) {
            log_error("server_init(%s, %hu): listen: %s", host, port, strerror(errno));
//            close(sockfd);
//            cursor = cursor->ai_next;
//            continue;
            goto failcontinue;
        }

        server_t *server = malloc(sizeof(server_t));
        *target = server;

        file_descriptor_t *fd = malloc(sizeof(file_descriptor_t));
        memset(fd, 0, sizeof(file_descriptor_t));
        fd->fd = sockfd;
        fd->handler_data = server;
        fd->read_handler = &server_handle_read;
        server->fd = fd;

        server->saddrlen = cursor->ai_addrlen;
        server->saddr = malloc(cursor->ai_addrlen);
        memcpy(server->saddr, cursor->ai_addr, cursor->ai_addrlen);
        goto done;

failcontinue:
        close(sockfd);
        cursor = cursor->ai_next;
    }

    success = false;

done:
    if (res) freeaddrinfo(res);
    return success;
}

#if 0
bool ensure_sockfile_avail(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        if (errno == ENOENT) return true;
        log_error("ensure_sockfile_avail(%s): stat: %s", path, strerror(errno));
        return false;
    }

    if (S_ISSOCK(st.st_mode)) {
        if (unlink(path) < 0) {
            log_error("ensure_sockfile_avail(%s): unlink: %s", path, strerror(errno));
            return false;
        }
        return true;
    } else {
        log_warn("ensure_sockfile_avail(%s): not a sockfile, not going to unlink", path);
        return false; // Do not delete a regular file
    }
}
#endif

bool server_init_unix(const char *path, server_t **target) {
    //if (!ensure_sockfile_avail(path)) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    // Make sure the addr path ends in a null terminator (strncpy will not do us this) --bigfoot
    strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);
    addr.sun_path[sizeof(addr.sun_path)-1] = '\0';

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_error("server_init_unix(%s): socket: %s", path, strerror(errno));
        return false;
    }

    if (make_fd_nonblock(sockfd) < 0) {
        log_error("server_init_unix(%s): fcntl: %s", path, strerror(errno));
        close(sockfd);
        return false;
    }

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("server_init_unix(%s): bind: %s", path, strerror(errno));
        if (errno == EADDRINUSE) log_error("Maybe a socket file was left behind from a previous invocation that exited uncleanly?");
        close(sockfd);
        return false;
    }

    if (listen(sockfd, 20) < 0) {
        log_error("server_init_unix(%s): listen: %s", path, strerror(errno));
        close(sockfd);
        return false;
    }

    server_t *server = malloc(sizeof(server_t));
    *target = server;

    file_descriptor_t *fd = malloc(sizeof(file_descriptor_t));
    memset(fd, 0, sizeof(file_descriptor_t));
    fd->fd = sockfd;
    fd->handler_data = server;
    fd->read_handler = &server_handle_read;
    server->fd = fd;

    server->saddrlen = sizeof(addr);
    server->saddr = malloc(sizeof(addr));
    memcpy(server->saddr, &addr, sizeof(addr));

    return true;
}

void server_free(server_t *server) {
    if (!server) return;
    if (server->fd) close(server->fd->fd);

    if (server->saddr && server->saddr->sa_family == AF_UNIX) {
        char *path = ((struct sockaddr_un *)server->saddr)->sun_path;
        if (unlink(path) < 0) {
            log_error("server_free: Unable to unlink '%s': %s", path, strerror(errno));
        }
    }

    free(server->fd);
    free(server->saddr);
    free(server);
}

void server_handle_read(file_descriptor_t *fd, void *handler_info) {
    server_t *server = handler_info;

    // accept clients until no longer possible
    // TODO: make a client struct that has a fd and send/recv queue
    struct sockaddr_storage saddr;
    socklen_t saddrlen = sizeof(saddr);
    int accfd = accept(fd->fd, (struct sockaddr *)&saddr, &saddrlen);
    close(accfd);

    if (saddr.ss_family == AF_INET6) {
        struct sockaddr_in6 *in6_saddr = (struct sockaddr_in6 *)&saddr;
        if (IN6_IS_ADDR_V4MAPPED(&(in6_saddr->sin6_addr))) {
            struct sockaddr_in newaddr;
            memset(&newaddr, 0, sizeof(newaddr));
            newaddr.sin_family = AF_INET;
            newaddr.sin_port = in6_saddr->sin6_port;
            memcpy(&newaddr.sin_addr, in6_saddr->sin6_addr.s6_addr + 12, 4);
            char addrbuf[256];
            log_info("v4mapped: %s:%hu", inet_ntop(AF_INET, &newaddr.sin_addr, addrbuf, 256), ntohs(newaddr.sin_port));
        }
    }

    switch (saddr.ss_family) {
        case AF_UNIX:
            log_info("Family: AF_UNIX - path: %s", ((struct sockaddr_un *)&saddr)->sun_path);
            break;
        case AF_INET:
        case AF_INET6:
            char addrbuf[256];
            void *addr;
            unsigned short port;
            if (saddr.ss_family == AF_INET) {
                addr = &(((struct sockaddr_in *)&saddr)->sin_addr);
                port = ntohs(((struct sockaddr_in *)&saddr)->sin_port);
            } else {
                addr = &(((struct sockaddr_in6 *)&saddr)->sin6_addr);
                port = ntohs(((struct sockaddr_in6 *)&saddr)->sin6_port);
            }
            log_info("Family: %s - addr: %s - port: %hu", saddr.ss_family == AF_INET ? "AF_INET" : "AF_INET6", inet_ntop(saddr.ss_family, addr, addrbuf, 256), port);
    }
}
