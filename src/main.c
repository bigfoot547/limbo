#include <locale.h>
#include "log.h"
#include "build_config.h"
#include "event.h"
#include "server.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

bool shutdown_server = false;

void con_read_handler(file_descriptor_t *fd, void *d) {
    log_info("Console readable! :D");
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
            break;
        } else {
            write(1, buf, numread);
            if (!strncmp(buf, "stop", 4)) {
                shutdown_server = true;
                log_info("Shutdown flag set. The program should be ending in <5 seconds.");
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
        log_debug("handle complete on thread %d", (int)(unsigned long long)arg);
    }
}

// TODO: Handle (ignore) SIGPIPE and handle SIGINT
int main() {
    setlocale(LC_ALL, "");

    log_setlevel(LOG_DEBUG);
    log_info("Running " PROJECT_NAME " version " VERSION_NAME);
    event_loop_init();
    server_t *serv = NULL, *serv1 = NULL;
    if (!server_init("::", 25565, &serv)) {
        log_error("Failed to bind serv.");
        return 1;
    }

    //if (!server_init("localhost", 25565, &serv1)) {
    //    log_error("failed aaa");
    //    return 1;
    //}

    event_loop_want(serv->fd, FD_WANT_READ);
//    event_loop_want(serv1->fd, FD_WANT_READ);

    file_descriptor_t fd;
    memset(&fd, 0, sizeof(fd));
    int newfd = 0;
    fd.fd = newfd;
    fd.read_handler = &con_read_handler;
    fd.error_handler = &con_error_handler;
    if (make_fd_nonblock(newfd) < 0) {
        log_error("error: %s", strerror(errno));
    }
    event_loop_want(&fd, FD_WANT_READ);

    pthread_t pt[10];
    for (int i = 0; i < 10; ++i) {
        pthread_create(pt + i, NULL, &io_worker, (void *)(unsigned long long)i);
    }

    for (int i = 0; i < 10; ++i) {
        pthread_join(pt[i], NULL);
    }

    server_free(serv);
    server_free(serv1);

    event_loop_close();

    return 0;
}
