#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "core.h"

char *get_user(const uid_t uid) {
    struct passwd *pws;
    if ((pws = getpwuid(uid)) != NULL) return (pws->pw_name);
    return NULL;
}

void get_proc_info(pid_t pid, char *buffer[], size_t buf_max) {
    char procfd_path[64] = {0x0};
    char buf_temp[256] = {0x0};
    FILE *fp = NULL;
    size_t i = 0;

    snprintf(procfd_path, sizeof(procfd_path), "/proc/%d/status", pid);
    if ((fp = fopen(procfd_path, "r")) == NULL) {
        log_error("Failed to open proc_fd: %s", strerror(errno));
        return;
    }

    while (fgets(buf_temp, sizeof(buf_temp), fp) != NULL && i < buf_max) {
        buf_temp[strlen(buf_temp) - 1] = '\0';
        if (buf_temp[0] != '\0') buffer[i] = strdup(buf_temp);
        buf_temp[0] = '\0';
        i++;
    }
    fclose(fp);
}

Str_t *get_proc_cmd(pid_t pid) {
    char procfd_path[64] = {0x0};
    char buf_temp[PATH_MAX] = {0x0};
    Str_t *cmd = NULL;
    int fd = EOF;

    snprintf(procfd_path, sizeof(procfd_path), "/proc/%d/cmdline", pid);
    if ((fd = open(procfd_path, O_RDONLY | O_NONBLOCK)) == -1) {
        log_error("Failed to open proc cmdline: %s", strerror(errno));
        return NULL;
    }

    int retry = 5;
    int ret = 0;
    while ((ret = read(fd, buf_temp, 1024)) <= 0) {
        if (errno != EAGAIN || --retry <= 0) {
            log_error("Failed to read proc cmdline: %s", strerror(errno));
            break;
        }
    }

    close(fd);
    if (ret <= 0) {
        return NULL;
    }

    cmd = malloc(sizeof(Str_t) * ret + 1);
    if (cmd == NULL) return NULL;

    memcpy(cmd->str, buf_temp, ret);
    cmd->len = ret;
    cmd->str[cmd->len] = '\0';
    return cmd;
}

void prep_cmd(Str_t *cmd) {
    if (cmd == NULL) return;
    size_t i = 0;
    while (i < cmd->len - 1) {
        if (cmd->str[i] == '\0') {
            cmd->str[i++] = ' ';
            continue;
        }
        i++;
    }
}

proc_info_t *tokenizer(char *buffer[]) {
    size_t i = 0;
    char *saveptr = NULL;
    char *token = NULL;
    proc_info_t *proc_info = NULL;
    if ((proc_info = calloc(0x1, sizeof(proc_info_t))) == NULL) {
        log_error("Failed to allocate memory: %s", strerror(errno));
        return NULL;
    }

    while (i < 11) {
        if (buffer[i] != NULL) {
            token = strtok_r(buffer[i], ":\t\r ", &saveptr);
            if (token == NULL) {
                log_debug("Failed to load proc info\n");
                raise(SIGTERM);
            }

            if (strncmp(token, "Name", 4) == 0) {
                token = strtok_r(NULL, "\t ", &saveptr);
                proc_info->name = strdup(token);
            }

            if (strncmp(token, "Umask", 5) == 0) {
                token = strtok_r(NULL, "\t ", &saveptr);
                proc_info->Umask = strdup(token);
            }

            if (strncmp(token, "State", 5) == 0) {
                token = strtok_r(NULL, "\t ", &saveptr);
                proc_info->state = strdup(saveptr);
            }

            if (strncmp(token, "Uid", 3) == 0) {
                token = strtok_r(NULL, "\t", &saveptr);
                proc_info->username = strdup(get_user((int) atol(token)));
            }
            free(buffer[i]);
            buffer[i] = NULL;
        }
        i++;
    }
    return proc_info;
}

void cleanup_procinfo(void *data) {
    proc_info_t *proc_info = (proc_info_t *) data;
    if (proc_info != NULL) {
        if (proc_info->date != NULL) free(proc_info->date);
        if (proc_info->cmd != NULL) free(proc_info->cmd);
        if (proc_info->username != NULL) free(proc_info->username);
        if (proc_info->name != NULL) free(proc_info->name);
        if (proc_info->state != NULL) free(proc_info->state);
        if (proc_info->Umask != NULL) free(proc_info->Umask);
        free(proc_info);
    }
}

char *get_locale_time(void) {
    char *buffer = NULL;
    if ((buffer = calloc(26, sizeof(char))) == NULL) {
        log_error("Failed to allocate memory: %s", strerror(errno));
        return NULL;
    }

    struct tm tm = *localtime(&(time_t){time(NULL)});
    asctime_r(&tm, buffer);
    if (buffer[0] == '\0') {
        free(buffer);
        return NULL;
    }

    buffer[strnlen(buffer, 26) - 1] = '\0';
    return buffer;
}
