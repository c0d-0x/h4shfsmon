#include "core.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#define LOGGER_IMPL

#include <errno.h>
#include <linux/fanotify.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <threads.h>
#include <unistd.h>

#include "event.h"
#include "logger.h"
// static int pidfd_get_pidfd(int pidfd, unsigned int flags) { return syscall(SYS_pidfd_getfd, pidfd, flags); }

h4sh_status_t check_lock(char *path_lock) {
    if (access(path_lock, F_OK) == 0) {
        fprintf(stderr, "An instance of h4shfsmon is already running\n");
        fprintf(stderr, "If no h4shfsmon instance is running, Delete '%s' file \n", LOCK_FILE);
        return H4SH_OKK;
    }

    fprintf(stderr, "No instance of h4shfsmon running\n");
    FILE *fp_lock = NULL;
    if ((fp_lock = fopen(path_lock, "w")) == NULL) {
        fprintf(stderr, "Failed to create lock file [ %s ]: %s\n", strerror(errno), path_lock);
        return H4SH_ERR;
    }

    fclose(fp_lock);
    return H4SH_OK;
}

void fan_event_handler(int fan_fd, FILE *fp_log) {
    event_t *event = NULL;

    event_t buf[256] = {0};
    // unsigned proc_buf[256] = {0};
    char *buffer[11] = {NULL};
    char path[PATH_MAX] = {0};
    h4sh_proc_info_t *proc_info = NULL;
    ssize_t ret_len = 0, path_len = 0, p_event = 0;
    char procfd_path[PATH_MAX] = {0};

    while (true) {
        ret_len = read(fan_fd, buf, sizeof(buf));
        if (ret_len == -1 && errno != EAGAIN) {
            log_debug("Failed to read fan_events");
            raise(SIGTERM);
        }

        /* Check if end of available data reached. */
        if (ret_len <= 0) break;
        int nb_events = ret_len / sizeof(event_t);

        /* Point to the first event in the buffer. */

        event = buf;

        /* Loop over all events in the buffer. */

        while (event != NULL && !(nb_events <= 0)) {
            /* Check that run-time and compile-time structures match. */

            if (event->meta.vers != FANOTIFY_METADATA_VERSION) {
                log_error("Mismatch of fanotify metadata version");
                raise(SIGTERM);
            }

            /* metadata->fd contains either FAN_NOFD, indicating a
               queue overflow, or a file descriptor (a nonnegative
               integer). Here, queue overflow is simply ignored. */

            // if (metadata->fd == FAN_NOFD) {
            // TODO: Handle queue overflow !!
            // }

            // NOTE: Path for proc info recon

            // /proc/<pid>/comm — process name, one read, cheap
            // /proc/<pid>/exe — symlink to the actual binary path
            // /proc/<pid>/cmdline — full command line with args
            // /proc/<pid>/status — UID, GID, parent PID, capability sets
            // /proc/<pid>/fd/ — open file descriptors
            // /proc/self/fdinfo/<pidfd>

            if (event->meta.fd >= 0) {
                if (event->meta.mask & FAN_OPEN) {
                    p_event = FAN_OPEN;
                    get_proc_info(event->meta.pid, buffer, 11);

                } else if (event->meta.mask & FAN_MODIFY) {
                    p_event = FAN_MODIFY;
                    get_proc_info(event->meta.pid, buffer, 11);
                } else if (event->meta.mask & FAN_NOPIDFD) {
                    log_error("Process Terminated: %s", strerror(errno));
                    continue;
                }

                /* Retrieve and print pathname of the accessed file. */
                snprintf(procfd_path, sizeof(procfd_path), "/proc/self/fd/%d", event->meta.fd);
                path_len = readlink(procfd_path, path, sizeof(path) - 1);
                if (path_len == -1) {
                    log_error("readlink: %s", strerror(errno));
                    close(event->pidfd.pidfd);
                    raise(SIGTERM);
                }

                path[path_len] = '\0';

                if ((proc_info = tokenizer(buffer)) == NULL) {
                    log_error("Failed to load effective process's info");
                    close(event->pidfd.pidfd);
                    raise(SIGTERM);
                }

                close(event->pidfd.pidfd);
                proc_info->file = path;
                proc_info->event = (p_event == FAN_MODIFY) ? "FILE_MODIFIED" : "FILE_ACCESSED";

                log_info("%s %s %s %s %s %s", proc_info->event, proc_info->name, proc_info->Umask, proc_info->state,
                         proc_info->username, proc_info->file);

                cleanup_procinfo(proc_info);
                close(event->meta.fd);
            }

            event++;
            nb_events--;
        }
    }

    fflush(fp_log);
}
