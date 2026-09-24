#ifndef EVENT_H
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/fanotify.h>

typedef struct {
    struct fanotify_event_metadata meta;
    struct fanotify_event_info_pidfd pidfd;
} event_t;

char *get_user(const uid_t uid);
bool modify_poll(int *epoll_fd, int fd, struct epoll_event *ev);
bool append_poll(int *epoll_fd, int fd, struct epoll_event *ev);
bool remove_poll(int *epoll_fd, int fd);
bool set_nonblocking(int fd);

#endif  // !EVENT_H
