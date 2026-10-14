#ifndef CORE_H
#define CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include "util.h"

#define LOG_FILE "cf_log.txt"
#define LOCK_FILE "cf.lock"
#define HASH_SIZE 256
#define PROC_INFO_HEAD 11
#define PROC_INFO_NAME_MAX 16
#define PROC_INFO_UMASK_MAX 4
#define PROC_INFO_STATE_MAX 2

typedef enum : int8_t {
    H4SH_OK = 0,
    H4SH_OKK = 1,
    H4SH_ERR = -1,
    H4SH_ERR_TIMEOUT = -2,
    H4SH_ERR_PROTOCOL = -3
} h4sh_status_t;

typedef enum : uint8_t {
    PROC_NAME,
    PROC_UMASK,
    PROC_STATE,
    PROC_UID,
    PROC_UNKNOWN
} proc_token_t;

typedef struct {
    char *raw[PROC_INFO_HEAD];
    volatile char *file;
    volatile char *event;
    volatile char *username;
    Str_t *cmd;
    char name[PROC_INFO_NAME_MAX];
    char state[PROC_INFO_STATE_MAX];
    char Umask[PROC_INFO_UMASK_MAX];
} proc_info_t;

typedef struct {
    char *path;
    float friq;  // for rate limiting
    uint64_t last_modified;
    uint64_t created;
    uint64_t size;
    Str_t *hash;
} h4sh_file_t;

proc_info_t *new_proc_info(void);
void cleanup_procinfo(void *data);
void get_proc_info(pid_t pid, char *seg[]);
void get_proc_cmd(pid_t pid, Str_t *cmd);
void prep_cmd(Str_t *cmd);

proc_info_t *tokenizer(proc_info_t *proc_info);
void fan_event_handler(int fan_fd, FILE *fp_log, proc_info_t *proc_info);
h4sh_status_t check_lock(char *path_lock);

#endif
