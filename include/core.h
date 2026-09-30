#ifndef CORE_H

#define CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define LOG_FILE "cf_log.txt"
#define LOCK_FILE "cf.lock"
#define CUSTOM_ERR (-1)
#define HASH_SIZE 256

#define FATAL(...)                    \
    do {                              \
        fprintf(stderr, "ERROR: ");   \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
        exit(EXIT_FAILURE);           \
    } while (0)

#define OUT_OF_MEMORY() FATAL("Out of memory")
#define GET_STR(obj) (obj) ? obj->str : NULL

typedef struct {
    size_t len;
    char str[];
} Str_t;

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
    Str_t *cmd;
    char *event;
    char *username;
    char *Umask;
    char *state;
} proc_info_t;

typedef struct {
    char *path;
    uint friq;  // for rate limiting
    uint64_t last_modified;
    uint64_t created;
    uint64_t size;
    Str_t *hash;
} h4sh_file_t;

void get_proc_info(pid_t pid, char *buffer[], size_t buf_max);
Str_t *get_proc_cmd(pid_t pid);
void pre_cmd(Str_t *cmd);

void cleanup_procinfo(proc_info_t *proc_info);
proc_info_t *tokenizer(char *buffer[]);
void fan_event_handler(int fan_fd, FILE *fp_log);
h4sh_status_t check_lock(char *path_lock);

#endif
