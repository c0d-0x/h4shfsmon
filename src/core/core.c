#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#define LOGGER_IMPL
#define UTIL_IMPL
#define USE_VECTORS

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fanotify.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/fanotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syscall.h>
#include <threads.h>
#include <unistd.h>
#include <xfs/handle.h>

#include "core.h"
#include "logger.h"
#include "util.h"

long get_file_size(char *path) {
    struct stat meta = {0};
    if (stat(path, &meta) != 0) {
        fprintf(stderr, "Error: [ %s ] Failed to read file metadata\n", path);
        return 0;
    }
    return meta.st_size;
}

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

static bool fid_to_path(struct fanotify_event_info_fid *fid, char *out, size_t out_len) {
    char fd_path[64] = {0};
    char dir[PATH_MAX] = {0};
    int fd = -1;

    struct file_handle *file_handle = (struct file_handle *) fid->handle;
    const char *name = (const char *) file_handle->f_handle + file_handle->handle_bytes;
    size_t name_len = (size_t) ((const char *) fid + fid->hdr.len - name);

    fd = open_by_handle_at(AT_FDCWD, file_handle, O_RDONLY | O_PATH | O_CLOEXEC);
    if (fd < 0) return false;

    snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);
    if (readlink(fd_path, dir, sizeof(dir) - 1) <= 0) {
        close(fd);
        log_error("Failed to readlink handle");
        return false;
    }

    close(fd);
    if (name_len == 0 || name[0] == '\0' || strcmp(name, ".") == 0) snprintf(out, out_len, "%s", dir);
    else snprintf(out, out_len, "%s/%s", dir, name);

    return true;
}

static const char *ev_name(uint64_t mask) {
    if (mask & FAN_RENAME) return "RENAMED";
    if (mask & FAN_MOVED_TO) return "MOVED_TO";
    if (mask & FAN_MOVED_FROM) return "MOVED_FROM";
    if (mask & FAN_DELETE) return "DELETED";
    if (mask & FAN_MODIFY) return "MODIFIED";
    if (mask & FAN_ACCESS) return "READ";
    return "UNKNOWN";
}

void fan_event_handler(int fan_fd, FILE *fp_log) {
    char buf[1096] __attribute__((aligned(__alignof__(struct fanotify_event_metadata)))) = {0};
    char *buffer[11] = {NULL};
    char path[PATH_MAX] = {0};
    proc_info_t *proc_info = NULL;
    Str_t *cmd = NULL;
    Vec_t *gb_collector = NULL;
    ssize_t ret_len = 0;

    while (true) {
        ret_len = read(fan_fd, buf, sizeof(buf));
        if (ret_len == -1) {
            if (errno == EAGAIN) break;
            if (errno == EINTR) continue;
            log_error("Failed to read fan_events: %s", strerror(errno));
            break;
        }

        if (ret_len == 0) break;

        struct fanotify_event_metadata *meta = (struct fanotify_event_metadata *) buf;
        for (; FAN_EVENT_OK(meta, ret_len); meta = FAN_EVENT_NEXT(meta, ret_len)) {
            bool have_path = false;
            int pidfd = -1;
            path[0] = '\0';

            if (meta->vers != FANOTIFY_METADATA_VERSION) {
                log_error("Mismatch of fanotify metadata version");
                vec_cleanup(gb_collector, cleanup_procinfo);
                vec_destroy(gb_collector);
                return;
            }

            if ((meta->mask & FAN_Q_OVERFLOW) != 0) {
                log_error("fanotify queue overflow");
                continue;
            }

            get_proc_info(meta->pid, buffer, 11);
            cmd = get_proc_cmd(meta->pid);

            struct fanotify_event_info_header *hdr
                = (struct fanotify_event_info_header *) ((char *) meta + meta->metadata_len);

            while ((char *) hdr < (char *) meta + meta->event_len) {
                switch (hdr->info_type) {
                    case FAN_EVENT_INFO_TYPE_DFID_NAME:
                    case FAN_EVENT_INFO_TYPE_OLD_DFID_NAME:
                        have_path = fid_to_path((struct fanotify_event_info_fid *) hdr, path, sizeof(path));
                        break;
                    case FAN_EVENT_INFO_TYPE_PIDFD: {
                        struct fanotify_event_info_pidfd *pi = (struct fanotify_event_info_pidfd *) hdr;
                        if (pi->pidfd >= 0) pidfd = pi->pidfd;
                        break;
                    }
                }
                hdr = (struct fanotify_event_info_header *) ((char *) hdr + hdr->len);
            }

            if (pidfd >= 0) close(pidfd);
            if (meta->fd >= 0) close(meta->fd);
            proc_info = tokenizer(buffer);
            if (proc_info == NULL) {
                log_error("Failed to load effective process's info");
                break;
            }

            if (cmd != NULL) prep_cmd(cmd);
            proc_info->cmd = cmd;
            proc_info->file = have_path ? path : NULL;
            proc_info->event = (char *) ev_name(meta->mask);

            log_info("%s %s %s %s %s %s %s", proc_info->event, proc_info->name, proc_info->Umask, proc_info->state,
                     proc_info->username, proc_info->file, GET_STR(cmd));

            vec_append(&gb_collector, (void *) proc_info);
            proc_info = NULL;
            cmd = NULL;
        }
    }

    vec_cleanup(gb_collector, cleanup_procinfo);
    vec_destroy(gb_collector);
    fflush(fp_log);
}
