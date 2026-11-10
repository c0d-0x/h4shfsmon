#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define UTILS_USE_VIEWS
#include "conf.h"
#include "logger.h"
#include "utils.h"

const View_t COMMENT = (View_t){.str = "#", .len = 1};
const View_t DIRS = (View_t){.str = "[dirs]", .len = 6};
const View_t HASH_TAG = (View_t){.str = "hash", .len = 4};
const View_t FILES = (View_t){.str = "[files]", .len = 7};
const View_t LEVEL_TAG = (View_t){.str = "level", .len = 5};
const View_t MOUNTS = (View_t){.str = "[mounts]", .len = 8};

static conf_tag_t tag_from_view(View_t *view) {
    if (view_starts_with(view, (View_t *) &COMMENT)) return TAG_COMMENT;
    if (view_eq((View_t *) &DIRS, view)) return TAG_DIRS;
    if (view_eq((View_t *) &FILES, view)) return TAG_FILES;
    if (view_eq((View_t *) &MOUNTS, view)) return TAG_MOUNTS;
    else return TAG_UNKNOWN;
}

static bool valid_file(char *path, int flag) {
    struct stat meta = {0};

    if (stat(path, &meta) != 0) {
        log_error("%s: cannot meta: %s", path, strerror(errno));
        return false;
    }

    if (meta.st_mode & flag) return true;
    else if (meta.st_mode & flag) return true;
    return false;
}

config_t *conf_parser(FILE *conf_fp) {
    char buffer[PATH_MAX] = {0};
    config_t *conf = NULL;

    if (fseek(conf_fp, 0, SEEK_SET) == -1) return NULL;
    if ((conf = calloc(0x1, sizeof(config_t))) == NULL) {
        log_fatal("Failed to allocate memory: %s", strerror(errno));
        raise(SIGTERM);
    }

    conf->len = 0;
    while (conf->len < MAX_WATCH) {
        if ((fgets(buffer, PATH_MAX, conf_fp)) == NULL) {
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';
        if (buffer[0] == '#' || (buffer[0] == '\n')) continue;

        View_t view = view_cstr(buffer);
        view = view_trim(&view);
        view_split(&view, '#', &view, NULL);
        if (view_empty(&view)) continue;

        char *path = NULL;
        switch (tag_from_view(&view)) {
            case TAG_COMMENT: continue;
            case TAG_DIRS:    {
                while ((fgets(buffer, PATH_MAX, conf_fp)) != NULL && conf->len < MAX_WATCH) {
                    buffer[strcspn(buffer, "\n")] = '\0';
                    View_t vbuf = view_cstr(buffer);

                    View_t left = {0};
                    View_t right = {0};
                    vbuf = view_trim(&vbuf);
                    if (view_empty(&vbuf)) break;

                    if (view_split(&vbuf, ' ', &left, &right)) {
                        left = view_trim(&left);
                        right = view_trim(&left);

                        path = left.str;
                        path[left.len + 1] = '\0';
                        if (view_starts_with((View_t *) &LEVEL_TAG, &right)) {
                            view_split(&right, ':', NULL, &right);
                            if (!view_to_u32(&right, &conf->watchlist[conf->len].prop.level))
                                conf->watchlist[conf->len].prop.level = 0;
                        }
                    } else {
                        path = vbuf.str;
                    }

                    if (!valid_file(path, S_IFDIR)) {
                        log_error("%s: Not a valid Folder", path);
                        continue;
                    }

                    conf->watchlist[conf->len].path = strdup(path);
                    conf->watchlist[conf->len++].type = F_IS_DIR;
                }
            } break;

            case TAG_FILES: {
                while ((fgets(buffer, PATH_MAX, conf_fp)) != NULL && conf->len < MAX_WATCH) {
                    buffer[strcspn(buffer, "\n")] = '\0';
                    View_t vbuf = view_cstr(buffer);
                    View_t left = {0};
                    View_t right = {0};

                    vbuf = view_trim(&vbuf);
                    if (view_empty(&vbuf)) break;
                    if (view_split(&vbuf, ' ', &left, &right)) {
                        left = view_trim(&left);
                        right = view_trim(&right);

                        path = left.str;
                        path[left.len + 1] = '\0';
                        if (view_eq((View_t *) &HASH_TAG, &right)) conf->watchlist[conf->len].prop.hash = true;
                    } else {
                        path = vbuf.str;
                    }

                    if (!valid_file(path, S_IFREG)) {
                        log_error("%s: Not a valid file", path);
                        continue;
                    }

                    conf->watchlist[conf->len].path = strdup(path);
                    conf->watchlist[conf->len++].type = F_IS_FILE;
                }
            } break;
            case TAG_MOUNTS: {
                while ((fgets(buffer, PATH_MAX, conf_fp)) != NULL && conf->len < MAX_WATCH) {
                    buffer[strcspn(buffer, "\n")] = '\0';
                    View_t vbuf = view_cstr(buffer);

                    vbuf = view_trim(&vbuf);
                    if (view_empty(&vbuf)) break;

                    path = vbuf.str;
                    if (!valid_file(path, S_IFDIR)) {
                        log_error("%s: Not a valid Folder", path);
                        continue;
                    }

                    conf->watchlist[conf->len].path = strdup(path);
                    conf->watchlist[conf->len++].type = F_IS_MNT;
                }
            } break;
            case TAG_UNKNOWN: {
                log_error("%s: Invalid entry in the config", buffer);
                continue;
            }
        }
    }

    if (conf->len == 0) {
        free(conf);
        conf = NULL;
    }

    return conf;
}

void conf_cleanup(config_t *conf) {
    log_debug("watchlist clean up");
    for (size_t i = 0; i < conf->len; i++) free(conf->watchlist[i].path);
    free(conf);
}
