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
#include <netinet/in.h>
#include <stdbool.h>

#include "log.h"
#include "client.h"

void server_handle_read(file_descriptor_t *fd, void *handler_info);

int make_fd_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) return flags;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

server_t *server_init_common(int sockfd, struct sockaddr *saddr, socklen_t saddrlen) {
/*        server_t *server = malloc(sizeof(server_t));
        memset(server, 0, sizeof(server_t));
        *target = server;

        file_descriptor_t *fd = malloc(sizeof(file_descriptor_t));
        memset(fd, 0, sizeof(file_descriptor_t));
        fd->fd = sockfd;
        fd->handler_data = server;
        fd->read_handler = &server_handle_read;
        server->fd = fd;

        server->saddrlen = cursor->ai_addrlen;
        memcpy(&server->saddr, cursor->ai_addr, cursor->ai_addrlen);*/
    server_t *server = malloc(sizeof(server_t));
    memset(server, 0, sizeof(server_t));

    file_descriptor_t *fd = malloc(sizeof(file_descriptor_t));
    memset(fd, 0, sizeof(file_descriptor_t));
    fd->fd = sockfd;
    fd->handler_data = server;
    fd->read_handler = &server_handle_read;
    server->fd = fd;

    server->saddrlen = saddrlen;
    memcpy(&server->saddr.base, saddr, saddrlen);
    return server;
}

bool server_init(const char *host, unsigned short port, unsigned flags, server_t **target) {
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

        // make the socket nonblocking
        if (make_fd_nonblock(sockfd) < 0) {
            log_error("server_init(%s, %hu): fcntl: %s", host, port, strerror(errno));
            close(sockfd);
            success = false;
            goto done;
        }

        // enable reuseaddr
        int reuseaddr = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) < 0) {
            log_warn("server_init(%s, %hu): setsockopt(SOL_SOCKET, SO_REUSEADDR, %d): %s", host, port, reuseaddr, strerror(errno));
        }

        // enable/disable IPV6_V6ONLY depending on the flag (different systems have different defaults, see ipv6(7))
        if (cursor->ai_family == AF_INET6) {
            int val = !!(flags & SERVER_IPV6ONLY);
            if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(int)) < 0) {
                log_warn("server_init(%s, %hu): setsockopt(IPPROTO_IPV6, IPV6_V6ONLY, %d): %s", host, port, val, strerror(errno));
            }
        } else if (flags & SERVER_IPV6ONLY) {
            log_warn("server_init(%s, %hu): server has flag SERVER_IPV6ONLY but host has family AF_INET, ignoring flag.", host, port);
        }

        // actually do the bind
        if (bind(sockfd, cursor->ai_addr, cursor->ai_addrlen) < 0) {
            log_error("server_init(%s, %hu): bind: %s", host, port, strerror(errno));
            goto failcontinue;
        }

        // and start listening for connections (none will be accepted until this is added to the event loop, though)
        if (listen(sockfd, 20) < 0) {
            log_error("server_init(%s, %hu): listen: %s", host, port, strerror(errno));
            goto failcontinue;
        }

        // wrap our file descriptor in a new server object
        *target = server_init_common(sockfd, cursor->ai_addr, cursor->ai_addrlen);
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

bool server_init_unix(const char *path, unsigned flags, server_t **target) {
    //if (!ensure_sockfile_avail(path)) return false;
    (void)flags;

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

    *target = server_init_common(sockfd, (struct sockaddr *)&addr, sizeof(addr));

    return true;
}

void server_free(server_t *server) {
    if (!server) return;
    if (server->fd) close(server->fd->fd);

    if (server->saddr.base.sa_family == AF_UNIX) {
        char *path = server->saddr.un.sun_path;
        if (unlink(path) < 0) {
            log_error("server_free: Unable to unlink '%s': %s", path, strerror(errno));
        }
    }

    free(server->fd);
    free(server);
}

void server_handle_read(file_descriptor_t *fd, void *handler_info) {
    server_t *server = (server_t *)handler_info;

    struct sockaddr_storage saddr;
    socklen_t saddrlen;
    int accattempt;
    while (true) {
        saddrlen = sizeof(saddr);
        accattempt = 0;
        int accfd = accept(fd->fd, (struct sockaddr *)&saddr, &saddrlen);
        if (accfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // no more peers to accept
                fd->state &= ~FD_CAN_READ;
                break;
            } else {
                log_error("server_handle_read: accept: %s (attempt %d)", strerror(errno), ++accattempt);
                if (accattempt >= 5) {
                    log_error("server_handle_read: Surpassed attempt 5 attempting to accept a peer.");
                    abort(); // FIXME: shut down cleanly?
                }
                continue;
            }
        }

        if (make_fd_nonblock(accfd) < 0) {
            log_error("server_handle_read: fcntl: %s", strerror(errno));
            close(accfd);
            continue;
        }

        if (!server->clients) {
            log_error("I've got nowhere to put my users! Closing connection %d.", accfd);
            close(accfd);
            continue;
        }

        client_t *client = client_init(accfd, (struct sockaddr *)&saddr, saddrlen);
        client->clients = server->clients;
        client->mypos = dll_addend(server->clients, client);
        event_loop_want(client->fd, FD_WANT_READ | FD_WANT_WRITE);
    }
}
