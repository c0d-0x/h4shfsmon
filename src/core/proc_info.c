#include "logger.h"

#include <errno.h>
#include <pwd.h>
#include <signal.h>
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
    char buf_temp[64] = {0x0};
    FILE *proc_fd = NULL;
    size_t i = 0;

    snprintf(procfd_path, sizeof(procfd_path), "/proc/%d/status", pid);
    if (access(procfd_path, F_OK) != 0) {
        log_error("Effective process has terminated");
        return;
    }

    if ((proc_fd = fopen(procfd_path, "r")) == NULL) {
        log_error("Failed to open proc_fd: %s", strerror(errno));
        return;
    }

    while (fgets(buf_temp, sizeof(buf_temp), proc_fd) != NULL && i < buf_max) {
        buf_temp[strlen(buf_temp) - 1] = '\0';
        if (buf_temp[0] != '\0') buffer[i] = strdup(buf_temp);
        i++;
    }
    fclose(proc_fd);
}

h4sh_proc_info_t *tokenizer(char *buffer[]) {
    size_t i = 0;
    char *saveptr = NULL;
    char *token = NULL;
    h4sh_proc_info_t *proc_info = NULL;
    if ((proc_info = calloc(0x1, sizeof(h4sh_proc_info_t))) == NULL) {
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
                proc_info->username = strdup(get_user(atoi(token)));
            }
            // log_debug("&buffer[%ld]: %p", i, buffer[i]);
            // log_debug("&token: %p\n", token);
            free(buffer[i]);
            buffer[i] = NULL;
        }
        i++;
    }
    return proc_info;
}

void cleanup_procinfo(h4sh_proc_info_t *proc_info) {
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
