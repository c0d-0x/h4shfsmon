/**
 * Copyright (c) 2025 JeepWay
 * HERE: https://github.com/JeepWay/logger-c
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LOG_H
#define LOG_H

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(__GNUC__) || defined(__clang__)
#define __LOGGER_HAS_TYPEOF 1
#else
#define __LOGGER_HAS_TYPEOF 0
#endif

#define MAX_HANDLERS 29
#define LOG_VERSION "0.1.0"
#define ROOT_HANDLER 0
#define ROOT_HANDLER_NAME "root"
#define DEFAULT NULL
#define DEFAULT_LEVEL LOG_TRACE
#define DEFAULT_STRAEM stderr
#define DEFAULT_FILE_NAME "logger/program.log"
#define DEFAULT_FILE_MODE "a"
#define DEFAULT_DATE_FORMAT1 "%H:%M:%S"                  // HH:MM:SS
#define DEFAULT_DATE_FORMAT2 "%d-%m-%Y"                  // DD-MM-YYYY
#define DEFAULT_DATE_FORMAT3 "%d/%m/%Y %H:%M:%S"         // DD/MM/YYYY HH:MM:SS
#define DEFAULT_DATE_FORMAT4 "%d-%m-%Y %H:%M:%S"         // DD-MM-YYYY HH:MM:SS
#define DEFAULT_DATE_FORMAT8 "%a, %d %b %Y %H:%M:%S %z"  // RFC 2822
#define DEFAULT_DATE_FORMAT9 "%Y-%m-%dT%H:%M:%S%z"       // ISO 8601

enum LOG_LEVEL : uint8_t {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

typedef struct record record_t;
typedef struct handler log_handler_t;
typedef struct logger logger_t;

typedef void (*log_dump_fn)(record_t *rec);
typedef void (*log_fmt_fn)(record_t *rec, const char *time_buf);
typedef void (*log_LockFn)(bool lock, void *fp);

struct record {
    va_list ap;        // parse
    struct tm *time;   // localtime
    uint8_t level;     // LOG_LEVEL
    const char *file;  // __FILE__
    size_t line;       // __LINE__
    const char *msg_fmt;
    const char *hd_name;
    log_fmt_fn hd_fmt_fn;
    void *hd_fp;
    const char *hd_date_fmt;
};

struct handler {
    const char *name;
    log_dump_fn dump_fn;
    log_fmt_fn fmt_fn;
    void *fp;
    uint8_t level;
    bool quiet;
    const char *date_fmt;
};

void init_logger(const char *date_fmt, bool quiet);
int log_add_file_handler(const char *filename, const char *filemode, uint8_t level, const char *name);
int log_add_stream_handler(FILE *fp, uint8_t level, const char *name);

void _log_message(uint8_t level, const char *file, int line, const char *msg_fmt, ...);
#define log_trace(...) _log_message(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) _log_message(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) _log_message(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) _log_message(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) _log_message(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) _log_message(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

// methods to set handler properties
#if __LOGGER_HAS_TYPEOF
#define _log_set_member(name, type, member, value)                                                                  \
    ({                                                                                                              \
        typeof(((struct handler *) 0)->member) __tmp = (value);                                                     \
        _log_set_attribute(name, #member, offsetof(struct handler, member), sizeof(((struct handler *) 0)->member), \
                           &__tmp);                                                                                 \
    })
#else
#define _log_set_member(name, type, member, value)                                                  \
    ({                                                                                              \
        type __tmp = (value);                                                                       \
        _log_set_attribute(name, #member, offsetof(struct handler, member), sizeof(__tmp), &__tmp); \
    })
#endif

void _log_set_attribute(const char *name, const char *, size_t offset, size_t size, void *value);
#define log_set_dump_fn(name, value) _log_set_member(name, log_dump_fn, dump_fn, value)
#define log_set_fmt_fn(name, value) _log_set_member(name, log_fmt_fn, fmt_fn, value)
#define log_set_level(name, value) _log_set_member(name, size_t, level, value)
#define log_set_quiet(name, value) _log_set_member(name, bool, quiet, value)
#define log_set_date_fmt(name, value) _log_set_member(name, const char *, date_fmt, value)

void log_set_lock(log_LockFn fn, void *fp);

// some default log_fmt_fn and log_dump_fn functions
void dump_log(record_t *rec);
void color_fmt1(record_t *rec, const char *time_buf);
void color_fmt2(record_t *rec, const char *time_buf);
void no_color_fmt1(record_t *rec, const char *time_buf);
void no_color_fmt2(record_t *rec, const char *time_buf);

// #define LOGGER_IMPL
#ifdef LOGGER_IMPL

struct logger {
    log_LockFn lock;
    log_handler_t handlers[MAX_HANDLERS];
    size_t count;
};

static logger_t L = {
    .lock = NULL,
    .count = 0,
};

static const char *level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

static const char *level_colors[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};

static void lock(void) {
    if (L.lock) {
        L.lock(true, L.handlers[0].fp);
    }
}

static void unlock(void) {
    if (L.lock) {
        L.lock(false, L.handlers[0].fp);
    }
}

void log_set_lock(log_LockFn fn, void *fp) {
    L.lock = fn;
    L.handlers[0].fp = fp;
}

void init_logger(const char *date_fmt, bool quiet) {
    L.handlers[ROOT_HANDLER] = (log_handler_t) {
        .name = ROOT_HANDLER_NAME,
        .dump_fn = dump_log,
        .fmt_fn = color_fmt1,
        .fp = DEFAULT_STRAEM,
        .level = DEFAULT_LEVEL,
        .quiet = quiet,
        .date_fmt = date_fmt,
    };
    L.count = 1;
}

static int log_add_handler(const char *name, log_dump_fn dump_fn, log_fmt_fn fmt_fn, void *fp, uint8_t level,
                           bool quiet, const char *date_fmt) {
    if (L.count == MAX_HANDLERS) {
        fprintf(DEFAULT_STRAEM, "[Logger C] Maximum number of handlers reached: %d\n", MAX_HANDLERS);
        return -1;
    }

    L.handlers[L.count++] = (log_handler_t) {
        .name = name,
        .dump_fn = dump_fn,
        .fmt_fn = fmt_fn,
        .fp = fp,
        .level = level,
        .quiet = quiet,
        .date_fmt = date_fmt,
    };
    return 0;
}

int log_add_file_handler(const char *filename, const char *filemode, uint8_t level, const char *name) {
    assert(level <= LOG_FATAL);
    assert(name);
    filename = filename ? filename : DEFAULT_FILE_NAME;
    filemode = filemode ? filemode : DEFAULT_FILE_MODE;
    FILE *fp = fopen(filename, filemode);

    if (!fp) {
        fprintf(DEFAULT_STRAEM, "[Logger C] Unable to open log file: %s\n", filename);
        return -1;
    }

    return log_add_handler(name, dump_log, no_color_fmt1, fp, level, false, DEFAULT_DATE_FORMAT3);
}

int log_add_stream_handler(FILE *fp, uint8_t level, const char *name) {
    assert(level <= LOG_FATAL);
    assert(name);
    fp = fp ? fp : DEFAULT_STRAEM;
    return log_add_handler(name, dump_log, color_fmt1, fp, level, false, DEFAULT_DATE_FORMAT1);
}

static void update_record(record_t *rec, log_handler_t *hd) {
    if (!rec->time) {
        time_t t = time(NULL);
        rec->time = localtime(&t);
    }
    rec->hd_name = hd->name;
    rec->hd_fmt_fn = hd->fmt_fn;
    rec->hd_fp = hd->fp;
    rec->hd_date_fmt = hd->date_fmt;
}

void _log_message(uint8_t level, const char *file, int line, const char *msg_fmt, ...) {
    lock();
    record_t rec = {
        .level = level,
        .file = file,
        .line = line,
        .msg_fmt = msg_fmt,
    };

    log_handler_t *rh = &L.handlers[ROOT_HANDLER];
    if (!rh->quiet && level >= rh->level) {
        update_record(&rec, rh);
        va_start(rec.ap, msg_fmt);
        rh->dump_fn(&rec);
        va_end(rec.ap);
    }

    for (size_t i = ROOT_HANDLER + 1; i < L.count && L.handlers[i].dump_fn; i++) {
        log_handler_t *hd = &L.handlers[i];
        if (!hd->quiet && level == hd->level) {
            update_record(&rec, hd);
            va_start(rec.ap, msg_fmt);
            hd->dump_fn(&rec);
            va_end(rec.ap);
        }
    }
    unlock();
}

static const char *modifiable_members = "dump_fn fmt_fn level quiet date_fmt";

void _log_set_attribute(const char *name, const char *member, size_t offset, size_t size, void *value) {
    name = name ? name : ROOT_HANDLER_NAME;

    if (!strstr(modifiable_members, member)) {
        fprintf(DEFAULT_STRAEM, "[Logger C] Handler's member can't be modified: %s\n", member);
        return;
    }

    for (size_t i = ROOT_HANDLER; i < L.count; i++) {
        if (strcmp(L.handlers[i].name, name) == 0) {
            memcpy((void *) &L.handlers[i] + offset, value, size);
            return;
        }
    }
    fprintf(DEFAULT_STRAEM, "[Logger C] Handler's name not found: %s\n", name);
}

void dump_log(record_t *rec) {
    char time_buf[32] = {0};
    strftime(time_buf, sizeof(time_buf), rec->hd_date_fmt, rec->time);
    rec->hd_fmt_fn(rec, time_buf);
    vfprintf(rec->hd_fp, rec->msg_fmt, rec->ap);
    fprintf(rec->hd_fp, "\n");
    fflush(rec->hd_fp);
}

void color_fmt1(record_t *rec, const char *time_buf) {
    static const char *fmt = "%s %s%-5s\x1b[0m ";
    fprintf(rec->hd_fp, fmt, time_buf, level_colors[rec->level], level_strings[rec->level]);
}

void color_fmt2(record_t *rec, const char *time_buf) {
    static const char *fmt = "%s (%s) %s%-5s\x1b[0m \x1b[90m[%s:%d]:\x1b[0m ";
    fprintf(rec->hd_fp, fmt, time_buf, rec->hd_name, level_colors[rec->level], level_strings[rec->level], rec->file,
            rec->line);
}

void no_color_fmt1(record_t *rec, const char *time_buf) {
    static const char *fmt = "%s %-5s ";
    fprintf(rec->hd_fp, fmt, time_buf, level_strings[rec->level]);
}

void no_color_fmt2(record_t *rec, const char *time_buf) {
    static const char *fmt = "%s (%s) %-5s [%s:%d]: ";
    fprintf(rec->hd_fp, fmt, time_buf, rec->hd_name, level_strings[rec->level], rec->file, rec->line);
}

#endif  // LOGGER_IMPL

#endif
