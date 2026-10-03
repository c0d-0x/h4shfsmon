#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "conf.h"
#include "logger.h"

config_t *conf_parser(int conf_fd) {
    // TODO: rewrite to new config syntax
    log_debug("Loading watchlist from the CONFIG_FILE: %s", CONFIG_FILE);

    char cc = 0;
    struct stat meta = {0};
    size_t i = 0, watch_len = 0, len = 0;
    uint8_t flag = 0;
    char buffer[PATH_MAX] = {0};
    config_t *conf = NULL;

    if (lseek(conf_fd, 0, SEEK_SET) == -1) return NULL;
    if ((conf = calloc(0x1, sizeof(config_t))) == NULL) {
        log_fatal("Failed to allocate memory: %s", strerror(errno));
        raise(SIGTERM);
    }

    while (watch_len < MAX_WATCH) {
        if ((len = read(conf_fd, &cc, sizeof(char))) <= 0) {
            break;
        }

        if (cc == 0x20 || (cc == '\n' && i == 0)) continue;
        if (cc == '\n') {
            if (buffer[0] == 0x023) {
                i = 0;
                continue;
            }

            buffer[i] = '\0';
            if (stat(buffer, &meta) != 0) {
                log_error("%s: Invalid File or Folder", buffer);
                i = 0;
                continue;
            }

            if (meta.st_mode & S_IFDIR) flag = F_IS_DIR;
            else if (meta.st_mode & S_IFREG) flag = F_IS_FILE;
            else flag = F_NT_FND;

            i = 0;
            (conf->watchlist[watch_len].path) = strdup(buffer);
            conf->watchlist[watch_len].type = flag;
            conf->watchlist_len = ++watch_len;
        } else buffer[i++] = cc;
    }

    if (conf->watchlist_len == 0) {
        free(conf);
        conf = NULL;
        return NULL;
    }

    return conf;
}

void conf_cleanup(config_t *conf) {
    log_debug("watchlist clean up");
    for (size_t i = 0; i < conf->watchlist_len; i++) free(conf->watchlist[i].path);
    free(conf);
}
