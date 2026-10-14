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
#include "util.h"

char *get_user(const uid_t uid) {
    struct passwd *pws;
    if ((pws = getpwuid(uid)) != NULL) return (pws->pw_name);
    return NULL;
}

static void alloc_segment(char *seg[], int size) {
    for (int i = 0; i < size; i++) {
        if ((seg[i] = malloc(sizeof(char) * 256)) == NULL) OUT_OF_MEMORY();
        seg[i][0] = '\0';
    }
}

static void reset_segment(char *seg[], int size) {
    for (int i = 0; i < size; i++) seg[i][0] = '\0';
}

static void cleanup_segment(char *seg[], int size) {
    for (int i = 0; i < size; i++) free(seg[i]);
}

proc_info_t *new_proc_info(void) {
    proc_info_t *proc_info = NULL;
    if ((proc_info = malloc(sizeof(proc_info_t))) == NULL) OUT_OF_MEMORY();
    if ((proc_info->cmd = malloc(sizeof(Str_t) + sizeof(char) * 1024)) == NULL) OUT_OF_MEMORY();
    alloc_segment(proc_info->raw, PROC_INFO_HEAD);
    return proc_info;
}

void cleanup_procinfo(void *data) {
    proc_info_t *proc_info = (proc_info_t *) data;
    if (proc_info != NULL) {
        free(proc_info->cmd);
        cleanup_segment(proc_info->raw, PROC_INFO_HEAD);
        free(proc_info);
    }
}

void get_proc_info(pid_t pid, char *seg[]) {
    char procfd_path[64] = {0x0};
    FILE *fp = NULL;

    snprintf(procfd_path, sizeof(procfd_path), "/proc/%d/status", pid);
    if ((fp = fopen(procfd_path, "r")) == NULL) {
        log_error("Failed to open proc_fd: %s", strerror(errno));
        return;
    }

    size_t i = 0;
    reset_segment(seg, PROC_INFO_HEAD);
    while (fgets(seg[i++], sizeof(char) * 256, fp) != NULL && i < PROC_INFO_HEAD);

    fclose(fp);
}

void get_proc_cmd(pid_t pid, Str_t *cmd) {
    char procfd_path[64] = {0x0};
    int fd = EOF;

    snprintf(procfd_path, sizeof(procfd_path), "/proc/%d/cmdline", pid);
    if ((fd = open(procfd_path, O_RDONLY | O_NONBLOCK)) == -1) {
        log_error("Failed to open proc cmdline: %s", strerror(errno));
        return;
    }

    int retry = 5;
    cmd->len = 0;
    cmd->str[0] = '\0';
    while ((cmd->len = read(fd, cmd->str, 1024)) <= 0) {
        if (errno != EAGAIN || --retry <= 0) {
            log_error("Failed to read proc cmdline: %s", strerror(errno));
            break;
        }
    }

    close(fd);
    if (cmd->len < 0) cmd->len = 0;
    cmd->str[cmd->len] = '\0';
}

void prep_cmd(Str_t *cmd) {
    if (cmd == NULL || cmd->len == 0) return;
    for (int i = 0; i < cmd->len - 2; i++)
        if (cmd->str[i] == '\0') cmd->str[i] = ' ';
}

static proc_token_t cstr_to_token(char *token) {
    if (strncmp(token, "Name", 4) == 0) return PROC_NAME;
    if (strncmp(token, "Umask", 5) == 0) return PROC_UMASK;
    if (strncmp(token, "State", 5) == 0) return PROC_STATE;
    if (strncmp(token, "Uid", 3) == 0) return PROC_UID;
    return PROC_UNKNOWN;
}

proc_info_t *tokenizer(proc_info_t *proc_info) {
    size_t i = 0;
    char *saveptr = NULL;
    char *str_token = NULL;

    while (i < PROC_INFO_HEAD) {
        if (proc_info->raw[i] != NULL && proc_info->raw[i][0] != '\0') {
            str_token = strtok_r(proc_info->raw[i], ":\t\r\n ", &saveptr);
            if (str_token == NULL) {
                log_debug("Failed to tokenize proc info\n");
                proc_info->raw[i++][0] = '\0';
                continue;
            }

            switch (cstr_to_token(str_token)) {
                case PROC_NAME:
                    if ((str_token = strtok_r(NULL, "\t\n\r ", &saveptr)) != NULL) strcpy(proc_info->name, str_token);
                    break;
                case PROC_STATE:
                    if ((str_token = strtok_r(NULL, "\t\n\r ", &saveptr)) != NULL) strcpy(proc_info->state, str_token);
                    break;
                case PROC_UID:
                    if ((str_token = strtok_r(NULL, "\t\n\r ", &saveptr)) != NULL) {
                        int uid = str_token ? (int) atol(str_token) : -1;
                        proc_info->username = (uid != -1) ? get_user(uid) : NULL;
                    }
                    break;
                case PROC_UMASK:
                    if ((str_token = strtok_r(NULL, "\t\n\r ", &saveptr)) != NULL) strcpy(proc_info->Umask, str_token);
                    break;

                default: break;
            }
        }
        i++;
    }

    return proc_info;
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
