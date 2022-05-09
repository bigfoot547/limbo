#include "event.h"
#include <string.h> // for memset
#include <unistd.h> // for close
#include <sys/types.h>
#include <sys/socket.h>

#ifdef SOCKET_ENGINE_EPOLL
#include <sys/epoll.h>
typedef struct epoll_event fd_event_t;
#endif

#define MAX_EVENTS (16)

int sefd;
fd_event_t events[MAX_EVENTS];

#include "log.h"
void event_loop_init() {
    memset(events, 0, sizeof(events));

#ifdef SOCKET_ENGINE_EPOLL
    sefd = epoll_create1(0);
#endif
}

void event_loop_handle(int timeout) {
#ifdef SOCKET_ENGINE_EPOLL
    int numevt = epoll_wait(sefd, events, MAX_EVENTS, timeout);
#endif

    fd_event_t *evt = events;
    for (int i = 0; i < numevt; ++i, ++evt) {
#ifdef SOCKET_ENGINE_EPOLL
        file_descriptor_t *fd = (file_descriptor_t *)(evt->data.ptr);

        if (evt->events & EPOLLERR) {
            int error;
            socklen_t optlen = sizeof(int);
            if (getsockopt(fd->fd, SOL_SOCKET, SO_ERROR, &error, &optlen) < 0) {
                error = -1;
            }

            if (fd->error_handler) (*fd->error_handler)(fd, error, fd->handler_data);
            else {
                log_warn("Error on socket (%d): %s", fd->fd, error == -1 ? "Unknown" : (error == 0 ? "Disconnected" : strerror(error)));
                event_loop_delfd(fd);
                close(fd->fd);
            }
            continue; // Ignore the rest of the flags if the socket is errored
        }

        if (evt->events & (EPOLLHUP | EPOLLRDHUP)) {
            if (fd->error_handler) (*fd->error_handler)(fd, 0, fd->handler_data);
            else {
                log_warn("Disconnect on socket %d", fd->fd);
                event_loop_delfd(fd);
                close(fd->fd); // should probably use shutdown() here, but this branch shoudln't be taken anyway
            }
            continue; // Ignore the rest of the flags if the peer hung up
        }

        if (evt->events & EPOLLIN) {
            fd->state |= FD_CAN_READ;
            if (fd->read_handler) (*fd->read_handler)(fd, fd->handler_data);
        }

        if (evt->events & EPOLLOUT) {
            fd->state |= FD_CAN_WRITE;
            if (fd->write_handler) (*fd->write_handler)(fd, fd->handler_data);
        }
#endif
    }
}

void event_loop_close() {
    close(sefd);
}

#ifdef SOCKET_ENGINE_EPOLL
unsigned gen_epoll_flags(unsigned flags) {
    unsigned ep = EPOLLET;
    if (flags & FD_WANT_READ) ep |= EPOLLIN;
    if (flags & FD_WANT_WRITE) ep |= EPOLLOUT;
    return ep;
}
#endif

void event_loop_want(file_descriptor_t *fd, unsigned flags) {
    flags &= FD_WANT_ALL;

    fd->state &= ~FD_WANT_ALL;
    fd->state |= flags;

#ifdef SOCKET_ENGINE_EPOLL
    int op;
    if (fd->state & FD_INTEREST) {
        op = EPOLL_CTL_MOD;
    } else {
        op = EPOLL_CTL_ADD;
        fd->state |= FD_INTEREST;
    }

    struct epoll_event evt;
    memset(&evt, 0, sizeof(evt));
    evt.events = gen_epoll_flags(fd->state);
    evt.data.ptr = (void *)fd;
    epoll_ctl(sefd, op, fd->fd, &evt);
#endif
}

void event_loop_delfd(file_descriptor_t *fd) {
    if (!(fd->state & FD_INTEREST)) return;

#ifdef SOCKET_ENGINE_EPOLL
    struct epoll_event evt; // older kernels want evt to be non-NULL
    memset(&evt, 0, sizeof(evt));
    epoll_ctl(sefd, EPOLL_CTL_DEL, fd->fd, &evt);
#endif

    fd->state &= ~FD_INTEREST;
}
