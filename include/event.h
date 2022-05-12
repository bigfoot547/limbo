#ifndef LIMBO_EVENT_H_INCLUDED
#define LIMBO_EVENT_H_INCLUDED

#define FD_WANT_READ  (1u << 0)
#define FD_WANT_WRITE (1u << 1)
#define FD_CAN_READ   (1u << 2)
#define FD_CAN_WRITE  (1u << 3)
#define FD_INTEREST   (1u << 4)
#define FD_CALL_COMPLETE (1u << 5)

#define FD_WANT_ALL (FD_WANT_READ | FD_WANT_WRITE)

void event_loop_init();
void event_loop_handle(int timeout);
void event_loop_close();

// Represents a single file descriptor, paired with whether it can be read to or written from
typedef struct tag_file_descriptor {
    int fd;
    unsigned state;

    void *handler_data;
    void (*read_handler)(struct tag_file_descriptor *fd, void *handler_data);
    void (*write_handler)(struct tag_file_descriptor *fd, void *handler_data);

    // Values for error:
    //   -1: Unknown error (error when retrieving the error from the fd?)
    //    0: No error
    // else: errno on the socket
    void (*error_handler)(struct tag_file_descriptor *fd, int error, void *handler_data);

    void (*handle_complete)(struct tag_file_descriptor *fd, void *handler_data);
} file_descriptor_t;

void event_loop_want(file_descriptor_t *fd, unsigned flags);
void event_loop_delfd(file_descriptor_t *fd);

#endif // include guard
