#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <sys/types.h>

#define CONFIG_FILE ".config/cf.conf"
#define CF_HOME_DIR ".config"
#define VERSION "v1.0.0"
#define MAX_WATCH 256

typedef enum : int8_t {
    F_NT_FND = -1,
    F_IS_FILE = 0,
    F_IS_DIR = 1,
    F_IS_MNT = 2

} fs_t;

typedef struct {
    fs_t type;
    char *path;
} watch_t;

typedef struct {
    size_t watchlist_len;
    watch_t watchlist[MAX_WATCH];
} config_t;

int init_inotify(char *file_path);
config_t *inotify_event_handler(int inotify_fd, int config_fd, config_t *(*handler)(int config_fd));

config_t *conf_parser(int config_fd);
void conf_cleanup(config_t *config_obj);

#endif  // !CONFIG_H
