#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "conf.h"
#include "core.h"
#include "logger.h"

int init_inotify(char *file_path) {
    int inotify_fd;
    inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd == -1) {
        log_error("Failed to initialize inotify");
        return CUSTOM_ERR;
    }

    if (inotify_add_watch(inotify_fd, file_path, IN_MODIFY | IN_CREATE) == -1) {
        log_error("Add Watch Failure: %s ", strerror(errno));
        return CUSTOM_ERR;
    }
    return inotify_fd;
}

config_t *inotify_event_handler(int inotify_fd, int config_fd, config_t *(*handler)(int config_fd)) {
    int len = 0;
    config_t *conf = NULL;
    struct inotify_event *event = NULL;
    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

    // TODO: Gaurds to watch only the config file.
    while ((len = read(inotify_fd, buffer, sizeof(buffer))) > 0) {
        for (char *buf_prt = buffer; buf_prt < buffer + len; buf_prt += sizeof(struct inotify_event) + event->len) {
            event = (struct inotify_event *) buf_prt;

            if ((event->mask & IN_MODIFY) || (event->mask & IN_CREATE)) {
                conf = handler(config_fd);

                if (inotify_add_watch(inotify_fd, CF_HOME_DIR, IN_MODIFY | IN_CREATE) == -1) {
                    log_error("Add Watch Failure: %s", strerror(errno));
                    raise(SIGTERM);
                }

                return conf;
            }
        }
    }

    if (len == -1 && errno != EAGAIN) {
        log_error("read syscall Failed: %s", strerror(errno));
        raise(SIGTERM);
    }
    return NULL;
}
