#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <pwd.h>
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

    int n = 0;
    while (fgets(buf_temp, sizeof(buf_temp), fp) != NULL && i < buf_max) {
        buffer[i][0] = '\0';
        if ((n = strlen(buf_temp)) == 0) continue;

        buf_temp[n - 1] = '\0';
        strcpy(buffer[i], buf_temp);

        buf_temp[0] = '\0';
        i++;
    }
    fclose(fp);
}

Str_t *get_proc_cmd(pid_t pid) {
    char procfd_path[64] = {0x0};
    char buff[PATH_MAX] = {0x0};
    Str_t *cmd = NULL;
    int fd = EOF;

    snprintf(procfd_path, sizeof(procfd_path), "/proc/%d/cmdline", pid);
    if ((fd = open(procfd_path, O_RDONLY | O_NONBLOCK)) == -1) {
        log_error("Failed to open proc cmdline: %s", strerror(errno));
        return NULL;
    }

    int retry = 5;
    int ret = 0;
    while ((ret = read(fd, buff, 1024)) <= 0) {
        if (errno != EAGAIN || --retry <= 0) {
            log_error("Failed to read proc cmdline: %s", strerror(errno));
            break;
        }
    }

    close(fd);
    if (ret <= 0) {
        return NULL;
    }

    cmd = malloc(sizeof(Str_t) + (sizeof(char) * ret) + 1);
    if (cmd == NULL) return NULL;

    memcpy(cmd->str, buff, ret);
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

static proc_token_t str_to_token(char *token) {
    if (strncmp(token, "Name", 4) == 0) return PROC_NAME;
    if (strncmp(token, "Umask", 5) == 0) return PROC_UMASK;
    if (strncmp(token, "State", 5) == 0) return PROC_STATE;
    if (strncmp(token, "Uid", 3) == 0) return PROC_UID;
    return PROC_UNKNOWN;
}

proc_info_t *tokenizer(char *buffer[], proc_info_t *proc_info) {
    size_t i = 0;
    char *saveptr = NULL;
    char *str_token = NULL;
    memset(proc_info, '\0', sizeof(proc_info_t));
    while (i < PROC_INFO_HEAD) {
        if (buffer[i] == NULL || buffer[i][0] == '\0') {
            i++;
            continue;
        }

        str_token = strtok_r(buffer[i], ":\t\r ", &saveptr);
        if (str_token == NULL) {
            i++;
            log_debug("Failed to tokenize proc info\n");
            continue;
        }

        switch (str_to_token(str_token)) {
            case PROC_NAME:
                if ((str_token = strtok_r(NULL, "\t ", &saveptr)) != NULL) strcpy(proc_info->name, str_token);
                else *proc_info->name = '\0';
                break;
            case PROC_STATE:
                if ((str_token = strtok_r(NULL, "\t ", &saveptr)) != NULL) strcpy(proc_info->state, str_token);
                else *proc_info->state = '\0';
                break;
            case PROC_UID:
                if ((str_token = strtok_r(NULL, "\t", &saveptr)) != NULL) {
                    int uid = str_token ? (int) atol(str_token) : -1;
                    proc_info->username = get_user(uid);
                }
                break;
            case PROC_UMASK:
                if ((str_token = strtok_r(NULL, "\t ", &saveptr)) != NULL) strcpy(proc_info->Umask, str_token);
                else *proc_info->Umask = '\0';
                break;

            default: break;
        }

        buffer[i++][0] = '\0';
    }
    return proc_info;
}

void cleanup_procinfo(void *data) {
    proc_info_t *proc_info = (proc_info_t *) data;
    if (proc_info != NULL) {
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
