#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define CONFIG_FILE ".config/cf.conf"
#define CF_HOME_DIR ".config"
#define VERSION "v1.0.0"
#define MAX_WATCH 256

typedef enum : int8_t {
    F_NOT_FOUND = -1,
    F_IS_FILE = 0,
    F_IS_DIR = 1,
    F_IS_MNT = 2
} conf_fs_t;

typedef struct {
    conf_fs_t type;
    char *path;
    union {
        uint32_t level;
        bool hash;
    } prop;
} conf_watch_t;

typedef enum : uint8_t {
    TAG_COMMENT,
    TAG_DIRS,
    TAG_FILES,
    TAG_MOUNTS,
    TAG_UNKNOWN,
} conf_tag_t;

typedef struct {
    size_t len;
    conf_watch_t watchlist[MAX_WATCH];
} config_t;

int init_inotify(char *file_path);
config_t *inotify_event_handler(int inotify_fd, FILE *config_fp, config_t *(*handler)(FILE *config_fp));

config_t *conf_parser(FILE *config_fp);
void conf_cleanup(config_t *config_obj);

#endif  // !CONFIG_H
