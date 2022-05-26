#include <locale.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

#include "log.h"
#include "build_config.h"
#include "event.h"
#include "server.h"
#include "list.h"
#include "client.h"
#include "macros.h"
#include "sched.h"
#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/fcntl.h>

/* TODO
    - Config file
    - Send chunks to client
    - Maximum fd (client) count configurable
    - Console handler for commands
    - Protocol compression
    - Handle bungeecord ip forwarding
    - Stop naively handling minecraft's encoding (iconv does not have java's fancy utf-8 thing)
    - Make all handler threads listen on a pipe so they can be woken up
*/

volatile bool shutdown_server = false;

void con_read_handler(file_descriptor_t *fd, void *d) {
    char buf[4096];
    ssize_t numread = 0;
    while (true) {
        numread = read(fd->fd, buf, 4096);
        if (numread == 0) {
            break; // eof reached
        } else if (numread == -1) {
            if (errno != EAGAIN) {
                log_error("Error reading from stdin: %s", strerror(errno));
                close(fd->fd);
            }
            fd->state &= ~FD_CAN_READ;
            break;
        } else {
            write(1, buf, numread);
            if (!strncmp(buf, "stop", 4)) {
                shutdown_server = true;
                event_loop_delfd(fd);
                close(fd->fd);
                log_info("Shutdown flag set. The program should be ending in <5 seconds.");
                break;
            }
        }
    }
}

void con_error_handler(file_descriptor_t *fd, int error, void *d) {
    log_warn("Error: %d", error);
}

void *io_worker(void *arg) {
    while (!shutdown_server) {
        event_loop_handle(5000);
    }
    log_debug("handle complete on thread %d", (int)(unsigned long long)arg);
    return NULL;
}

// Seconds for non-play protocol timeout
#define CONFIG_NPLAY_TIMEOUT (15)

// Seconds for play timeout
#define CONFIG_PLAY_TIMEOUT  (30)

// Player keep-alive frequency (should be < 20)
#define CONFIG_PING_FREQ     (5)

void *tick_worker(void *cl) {
    dllist_t *clients = cl;

    timer_state_t ts;
    if (sched_timer_init(&ts, 0, 500000000l) < 0) {
        log_error("tick_worker: sched_timer_init failed: %s", strerror(errno));
        log_error("tick_worker: timer could not be initialized, aborting.");
        abort();
    }

    client_t *curcli;
    struct timespec now, diff;
    while (!shutdown_server) {
        if (sched_timer_wgettime(CLOCK_MONOTONIC, &now) < 0) {
            log_error("tick_worker: sched_timer_wgettime(&now) failed (funny stuff is about to occur): %s", strerror(errno));
#ifdef BUILD_DEBUG
            abort();
#endif
        }

// helper macro to disconnect a client without breaking stuff
#define CLIENT_DISCONNECT(_cli, _fmt, ...)         \
client_disconnect((_cli), (_fmt), ## __VA_ARGS__); \
                                                   \
pthread_mutex_unlock(&(_cli)->evtmutex);           \
pthread_mutex_lock(&(_cli)->evtmutex);             \
pthread_mutex_unlock(&(_cli)->evtmutex);           \
                                                   \
client_free((_cli));

        DLLIST_FOREACH(clients, cur) {
            curcli = cur->ptr;
            pthread_mutex_lock(&curcli->evtmutex);

            sched_timespec_sub(&now, &curcli->lastping, &diff);
            if (curcli->protocol < PROTOCOL_PLAY && diff.tv_sec >= CONFIG_NPLAY_TIMEOUT) {
                CLIENT_DISCONNECT(curcli, "Ping timeout: %ld seconds", diff.tv_sec);
                continue;
            } else if (curcli->protocol == PROTOCOL_PLAY) {
                if (curcli->pingrespond && diff.tv_sec >= CONFIG_PING_FREQ) {
                    struct packet_play_keep_alive pkt = {
                        .id = PKTID_WRITE_PLAY_KEEP_ALIVE,
                        .payload = (int32_t)clock()
                    };
                    client_write_pkt(curcli, &pkt);
                } else if (!curcli->pingrespond && diff.tv_sec >= CONFIG_PLAY_TIMEOUT) {
                    CLIENT_DISCONNECT(curcli, "Ping timeout: %ld seconds", diff.tv_sec);
                    continue;
                }
            }

            pthread_mutex_unlock(&curcli->evtmutex);
        } DLLIST_FOREACH_DONE(clients);

#undef CLIENT_DISCONNECT

        if (sched_timer_wait(&ts) < 0) {
            log_error("tick_worker: sched_timer_wait failed: %s", strerror(errno));
#ifdef BUILD_DEBUG
            abort();
#endif
        }
    }

    log_debug("tick_worker complete");
    return NULL;
}

// TODO: Handle (ignore) SIGPIPE and handle SIGINT
int main(void) {
    setlocale(LC_ALL, "");

    log_setlevel(LOG_DEBUG);
    log_info("Running " PROJECT_NAME " version " VERSION_NAME);

    dllist_t *clients = dll_create_sync();

    event_loop_init();
    server_t *serv = NULL;
    if (!server_init("::", 25565, 0, &serv)) {
        log_error("Failed to bind serv.");
        return 1;
    }

    serv->clients = clients;

    event_loop_want(serv->fd, FD_WANT_READ);

    file_descriptor_t fd;
    memset(&fd, 0, sizeof(fd));
    int newfd = dup(STDIN_FILENO);
    fd.fd = newfd;
    fd.read_handler = &con_read_handler;
    fd.error_handler = &con_error_handler;
    if (fcntl(newfd, F_SETFL, fcntl(newfd, F_GETFL) | O_NONBLOCK) < 0) {
        log_error("error: %s", strerror(errno));
    }
    event_loop_want(&fd, FD_WANT_READ);

#define THREAD_CNT (10)
    pthread_t pt[THREAD_CNT + 1];
    for (int i = 0; i < THREAD_CNT; ++i) {
        pthread_create(pt + i, NULL, &io_worker, (void *)(unsigned long long)i);
    }
    pthread_create(pt + THREAD_CNT, NULL, &tick_worker, clients);

    for (int i = 0; i < THREAD_CNT+1; ++i) {
        pthread_join(pt[i], NULL);
    }
    //(void)io_worker(NULL);

    event_loop_delfd(serv->fd);
    server_free(serv);

    DLLIST_FOREACH(clients, cur) {
        client_t *cli = cur->ptr;
        client_disconnect(cli, "Shutting down");
        client_free(cli);
    } DLLIST_FOREACH_DONE(clients);
    dll_free(clients);

    event_loop_close();

    return 0;
}
