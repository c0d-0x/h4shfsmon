
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <time.h>
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fanotify.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>

#include "conf.h"
#include "core.h"
#include "logger.h"

int config_fd;
bool debug = true;
FILE *fp_log = NULL;
int fan_fd, inotify_fd;
config_t *conf = NULL;

void help(char *argv);
h4sh_status_t update_watchlist();
void signal_handler(int sig);
static void parse_options(const int argc, char *argv[]);
static void fan_mark_wraper(int fd, config_t *config_obj);

int main(int argc, char *argv[]) {
    nfds_t nfds = 0;
    int poll_num = 0;
    FILE *fp_lock = NULL;
    struct pollfd fds[2];
    struct sigaction sigact = {0};

    parse_options(argc, argv);
    if (check_lock(LOCK_FILE) != H4SH_OK) return EXIT_FAILURE;

    // if (!debug) DAEMONIZE();
    init_logger(DEFAULT_DATE_FORMAT3, !debug);
    if ((fp_lock = fopen(LOCK_FILE, "w")) == NULL) {
        log_debug("Failed to open %s file: %s", LOG_FILE, strerror(errno));
        return EXIT_FAILURE;
    }

    fprintf(fp_lock, "%d", getpid());
    fclose(fp_lock);

    log_debug("h4shfsmon started");
    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = signal_handler;
    sigact.sa_flags = SA_RESTART;

    if (sigaction(SIGTERM, &sigact, NULL) != 0 || sigaction(SIGINT, &sigact, NULL) != 0) {
        log_debug("%s", "Fail to make reception for signals");
        return EXIT_FAILURE;
    }

    log_debug("%s", "Made reception for signals");
    config_fd = open(CONFIG_FILE, O_RDONLY | O_NONBLOCK);
    if (config_fd == -1) {
        log_debug("Failed to open the config file: %s", CONFIG_FILE);
        raise(SIGTERM);
    }

    log_debug("%s", "Opening config file: %s", CONFIG_FILE);
    log_add_file_handler(LOG_FILE, "a+", LOG_INFO, "INFO_LOGS");
    log_debug("LOG_FILE opened: %s", LOG_FILE);
    log_debug("Initializing an Fa_Notify instance");

    /*fanotify for mornitoring files.*/
    fan_fd = fanotify_init(FAN_CLOEXEC | FAN_NONBLOCK | FAN_REPORT_PIDFD, O_RDONLY | O_LARGEFILE);
    if (fan_fd == -1) {
        log_debug("Failed to initializing an Fa_Notify instance: %s", strerror(errno));
        raise(SIGTERM);
    }

    /*Watch the config dir for changes*/
    inotify_fd = init_inotify(CF_HOME_DIR);
    if (inotify_fd == -1) raise(SIGTERM);

    log_debug("Initialized fanotify backend");
    conf = conf_parser(config_fd);
    if (conf == NULL || conf->watchlist_len == 0 || conf->watchlist->path == NULL) {
        log_error("%s: Add valid files and dirs to be watched", CONFIG_FILE);
        raise(SIGTERM);
    }

    log_debug("Marking watchlist");
    fan_mark_wraper(fan_fd, conf);
    conf_cleanup(conf);
    nfds = 2;
    fds[0].fd = fan_fd;
    fds[0].events = POLLIN;

    fds[1].fd = inotify_fd;
    fds[1].events = POLLIN;

    log_debug("Listening for events\n");
    while (true) {
        poll_num = poll(fds, nfds, -1);
        if (poll_num == -1) {
            if (errno == EINTR) continue;
            log_error("Poll Failed: %s", strerror(errno));
            raise(SIGTERM);
        }

        if (poll_num > 0) {
            if (fds[0].revents & POLLIN) fan_event_handler(fan_fd, fp_log);
            if (fds[1].revents & POLLIN) update_watchlist();
        }
    }
}

h4sh_status_t update_watchlist() {
    if ((conf = inotify_event_handler(inotify_fd, config_fd, conf_parser)) == NULL) return H4SH_ERR;
    log_debug("CONFIG_FILE: %s Modified", CONFIG_FILE);
    log_debug("Flushing  watchlist");
    if (fanotify_mark(fan_fd, FAN_MARK_FLUSH, FAN_OPEN | FAN_MODIFY | FAN_EVENT_ON_CHILD, AT_FDCWD, NULL) == -1) {
        log_error("Fanotify_Mark: Failed!!!");
        raise(SIGTERM);
    }

    fan_mark_wraper(fan_fd, conf);
    conf_cleanup(conf);
    return H4SH_OK;
}

static void fan_mark_wraper(int fd, config_t *config_obj) {
    for (size_t i = 0; i < config_obj->watchlist_len; i++) {
        if (fanotify_mark(fd, (config_obj->watchlist[i].f_type) ? FAN_MARK_ADD | FAN_MARK_ONLYDIR : FAN_MARK_ADD,
                          FAN_OPEN | FAN_MODIFY | FAN_EVENT_ON_CHILD, AT_FDCWD, config_obj->watchlist[i].path)
            == -1) {
            log_error("Failed to mark files from config");
            raise(SIGTERM);
        }
        log_debug("%s: Marked", config_obj->watchlist[i].path);
    }
}

void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        remove(LOCK_FILE);
        if (fp_log != NULL) fclose(fp_log);
        close(config_fd);
        log_debug("%s", "Terminating h4shfsmon");
        exit(EXIT_SUCCESS);
    }
}

void help(char *argv) {
    fprintf(stdout, "%s < --option >", argv);
    fprintf(stdout, "options\n -d: debug mode will prevent h4shfsmon from as  a daemon process\n");
}

static void parse_options(const int argc, char *argv[]) {
    if (getuid() != 0) {
        fprintf(stderr, "Run %s as root!!\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc > 1) {
        if (strncmp("-d", argv[1], 3) == 0) {
            debug = true;
            return;
        }
    }

    help(argv[0]);
    exit(EXIT_SUCCESS);
}
