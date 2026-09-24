#ifndef CORE_H

#define CORE_H
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define LOG_FILE "cf_log.txt"
#define LOCK_FILE "cf.lock"
#define CUSTOM_ERR (-1)

typedef enum : int8_t {
    H4SH_OK = 0,
    H4SH_OKK = 1,
    H4SH_ERR = -1,
    H4SH_ERR_TIMEOUT = -2,
    H4SH_ERR_PROTOCOL = -3
} h4sh_status_t;

typedef struct {
    char *date;
    char *file;
    char *name;
    char *cmd;
    char *event;
    char *username;
    char *Umask;
    char *state;
} h4sh_proc_info_t;

void get_proc_info(pid_t pid, char *buffer[], size_t buf_max);
void cleanup_procinfo(h4sh_proc_info_t *proc_info);
h4sh_proc_info_t *tokenizer(char *buffer[]);
void fan_event_handler(int fan_fd, FILE *fp_log);
h4sh_status_t check_lock(char *path_lock);
#endif
