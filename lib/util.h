#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define VEC_MAX 8
#define NOT_FOUND (-1)
#define QUEUE_MAX 8

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

#ifdef USE_VECTORS

// Vector
typedef struct {
    size_t size;
    size_t capacity;
    void *data[];
} Vec_t;

void *vec_pop(Vec_t *vec);
void vec_destroy(Vec_t *vec);
void ver_cleanup(Vec_t *vec, void (*cleanup_callback)(void *data));
bool vec_append(Vec_t **vec, void *data);

#endif  //! USE_VECTORS

#ifdef USE_VIEWS
// String view
typedef struct {
    int64_t len;
    char *str;
} View_t;

View_t view(const char *data, int64_t len);
View_t view_cstr(const char *str);

View_t view_slice(View_t *view, int64_t offset, int64_t len);
View_t view_skip(View_t *view, int64_t n);

bool view_starts_with(View_t *view, View_t *prefix);
bool view_eq(View_t *viewa, View_t *viewb);
bool view_empty(View_t *view);

int64_t view_find(View_t *view, View_t *needle, int64_t start_offset);
int64_t view_find_char(View_t *view, char c, int64_t start_offset);

bool view_ends_with(View_t *view, View_t *suffix);
bool view_split(View_t *view, char delim, View_t *left, View_t *right);

View_t view_trim(View_t *view);
View_t view_trim_left(View_t *view);
View_t view_trim_right(View_t *view);

bool view_eq_case(View_t *viewa, View_t *viewb);
bool view_starts_with_case(View_t *view, View_t *prefix);
bool view_to_u32(View_t *view, uint32_t *out);
char *view_to_cstr(View_t *view);

#endif  //! USE_VIEWS

#ifdef USE_QUEUES
// Queue
typedef struct {
    int size;
    int head;
    int tail;
    int capacity;
    void *data[];
} Queue_t;

bool enqueue(Queue_t **queue, void *data);
void *dequeue(Queue_t *queue);
bool queue_full(Queue_t *queue);
bool queue_empty(Queue_t *queue);
void queue_free(Queue_t **queue);
#endif  //! USE_QUEUES

// #define UTIL_IMPL
#ifdef UTIL_IMPL

#ifdef USE_VIEWS

static inline char ascii_tolower(char cc) {
    if (cc >= 'A' && cc <= 'Z') return cc + ('a' - 'A');
    return cc;
}

static inline bool ascii_isspace(char cc) {
    return cc == ' ' || cc == '\t' || cc == '\r' || cc == '\n' || cc == '\v' || cc == '\f';
}

bool view_empty(View_t *view) { return (view == NULL || view->str == NULL || view->len == 0); }

View_t view(const char *data, int64_t len) {
    if (data == NULL) return (View_t) {.str = NULL, .len = 0};
    return (View_t) {.str = (char *) data, .len = len};
}

View_t view_cstr(const char *str) {
    if (str == NULL) return (View_t) {.str = NULL, .len = 0};
    return (View_t) {.str = (char *) str, .len = strlen(str)};
}

View_t view_slice(View_t *view, int64_t offset, int64_t len) {
    if (view == NULL || view->str == NULL || offset >= view->len) return (View_t) {.str = NULL, .len = 0};

    int64_t rem = view->len - offset;
    if (len > rem) len = rem;
    return (View_t) {.str = view->str + offset, .len = len};
}

View_t view_skip(View_t *view, int64_t n) {
    if (view == NULL || view->str == NULL || n >= view->len) return (View_t) {.str = NULL, .len = 0};
    return (View_t) {.str = view->str + n, .len = view->len - n};
}

bool view_starts_with(View_t *view, View_t *prefix) {
    if (prefix == NULL || prefix->len == 0) return true;
    if (view == NULL || view->str == NULL || view->len < prefix->len || prefix->str == NULL) return false;

    return memcmp(view->str, prefix->str, prefix->len) == 0;
}

bool view_eq(View_t *viewa, View_t *viewb) {
    bool empty_a = view_empty(viewa);
    bool empty_b = view_empty(viewb);

    if (empty_a && empty_b) return true;
    if (empty_a || empty_b) return false;
    if (viewa->len != viewb->len) return false;

    return memcmp(viewa->str, viewb->str, viewa->len) == 0;
}

bool view_ends_with(View_t *view, View_t *suffix) {
    if (suffix == NULL || suffix->len == 0) return true;
    if (view == NULL || view->str == NULL || view->len < suffix->len || suffix->str == NULL) return false;

    int64_t offset = view->len - suffix->len;
    return memcmp(view->str + offset, suffix->str, suffix->len) == 0;
}

int64_t view_find(View_t *view, View_t *dlm, int64_t start_offset) {
    if (view == NULL || view->str == NULL || dlm == NULL || dlm->str == NULL) return NOT_FOUND;
    if (dlm->len == 0) return start_offset <= view->len ? start_offset : NOT_FOUND;
    if (start_offset >= view->len || view->len - start_offset < dlm->len) return NOT_FOUND;

    int64_t search_limit = view->len - dlm->len;
    for (int64_t i = start_offset; i <= search_limit; i++) {
        if (memcmp(view->str + i, dlm->str, dlm->len) == 0) return i;
    }

    return NOT_FOUND;
}

int64_t view_find_char(View_t *view, char cc, int64_t start_offset) {
    if (view == NULL || view->str == NULL || start_offset >= view->len) return NOT_FOUND;

    for (int64_t i = start_offset; i < view->len; i++) {
        if (view->str[i] == cc) return i;
    }

    return NOT_FOUND;
}

bool view_split(View_t *view, char delim, View_t *left, View_t *right) {
    if (view == NULL || view->str == NULL || left == NULL || right == NULL) return false;

    int64_t idx = view_find_char(view, delim, 0);
    if (idx == NOT_FOUND) {
        *left = *view;
        *right = (View_t) {.str = NULL, .len = 0};
        return false;
    }

    *left = (View_t) {.str = view->str, .len = idx};
    if (idx + 1 >= view->len) *right = (View_t) {.str = NULL, .len = 0};
    else *right = (View_t) {.str = view->str + (idx + 1), .len = view->len - (idx + 1)};

    return true;
}

View_t view_trim(View_t *view) {
    View_t ltrimmed = view_trim_left(view);
    return view_trim_right(&ltrimmed);
}

View_t view_trim_left(View_t *view) {
    if (view == NULL || view->str == NULL) return (View_t) {.str = NULL, .len = 0};

    int64_t start = 0;
    while (start < view->len && ascii_isspace(view->str[start++]));

    if (start >= view->len) return (View_t) {.str = NULL, .len = 0};
    return (View_t) {.str = view->str + start, .len = view->len - start};
}

View_t view_trim_right(View_t *view) {
    if (view == NULL || view->str == NULL || view->len == 0) return (View_t) {.str = NULL, .len = 0};

    int64_t end = view->len;
    while (end > 0 && ascii_isspace(view->str[--end]));

    if (end == 0) return (View_t) {.str = NULL, .len = 0};
    return (View_t) {.str = view->str, .len = end};
}

bool view_eq_case(View_t *viewa, View_t *viewb) {
    bool empty_a = view_empty(viewa);
    bool empty_b = view_empty(viewb);

    if (empty_a && empty_b) return true;
    if (empty_a || empty_b) return false;
    if (viewa->len != viewb->len) return false;
    for (int64_t i = 0; i < viewa->len; i++) {
        if (ascii_tolower(viewa->str[i]) != ascii_tolower(viewb->str[i])) return false;
    }

    return true;
}

bool view_starts_with_case(View_t *view, View_t *prefix) {
    if (prefix == NULL || prefix->len == 0) return true;
    if (view == NULL || view->str == NULL || view->len < prefix->len || prefix->str == NULL) return false;

    for (int64_t i = 0; i < prefix->len; i++) {
        if (ascii_tolower(view->str[i]) != ascii_tolower(prefix->str[i])) return false;
    }

    return true;
}

bool view_to_u32(View_t *view, uint32_t *out) {
    if (view == NULL || view->str == NULL || view->len == 0 || out == NULL) return false;

    uint64_t val = 0;
    for (int64_t i = 0; i < view->len; i++) {
        char cc = view->str[i];
        if (cc < '0' || cc > '9') return false;

        val = val * 10 + (cc - '0');
        if (val > UINT32_MAX) return false;
    }

    *out = (uint32_t) val;
    return true;
}

char *view_to_cstr(View_t *view) {
    if (view == NULL || view->str == NULL || view->len == 0) return NULL;

    char *dest = malloc(view->len + 1);
    if (dest == NULL) return NULL;
    memcpy(dest, view->str, view->len);
    dest[view->len] = '\0';
    return dest;
}

#endif  //! USE_VIEWS

#ifdef USE_QUEUES

// Queues
bool enqueue(Queue_t **queue, void *data) {
    if (data == NULL) return false;
    if (queue_empty(*queue)) {
        if ((*queue = malloc(sizeof(Queue_t) + sizeof(data) * QUEUE_MAX)) == NULL) {
            return false;
        }

        (*queue)->capacity = QUEUE_MAX;
        (*queue)->size = 0;
        (*queue)->head = 0;
        (*queue)->tail = 0;
    }

    if (queue_full(*queue)) {
        Queue_t *tmp = realloc(*queue, sizeof(Queue_t) + sizeof(data) * (*queue)->capacity * 2);
        if (tmp == NULL) OUT_OF_MEMORY();

        *queue = tmp;
        (*queue)->capacity *= 2;
    }

    (*queue)->data[(*queue)->tail] = data;
    (*queue)->tail = ((*queue)->tail + 1) % (*queue)->capacity;
    (*queue)->size++;

    return true;
}

void *dequeue(Queue_t *queue) {
    if (queue_empty(queue)) return NULL;

    void *data = queue->data[queue->head];
    queue->head = (queue->head + 1) % queue->size;
    return data;
}

bool queue_full(Queue_t *queue) { return queue->capacity == queue->size && queue->capacity != 0; }

bool queue_empty(Queue_t *queue) { return queue == NULL || queue->size == 0; }

void queue_free(Queue_t **queue) {
    free(*queue);
    *queue = NULL;
}

#endif  //! USE_QUEUES

#ifdef USE_VECTORS
// Vectors

bool vec_append(Vec_t **vec, void *data) {
    if (data == NULL) return false;
    if (*vec == NULL) {
        if ((*vec = malloc(sizeof(Vec_t) + sizeof(data) * VEC_MAX)) == NULL) OUT_OF_MEMORY();

        (*vec)->capacity = VEC_MAX;
        (*vec)->size = 0;
    } else if ((*vec)->size == (*vec)->capacity) {
        size_t capacity = (*vec)->capacity * 2;
        Vec_t *new_vec = realloc(*vec, sizeof(Vec_t) + sizeof(data) * capacity);
        if (new_vec == NULL) OUT_OF_MEMORY();

        *vec = new_vec;
        (*vec)->capacity = capacity;
    }

    (*vec)->data[(*vec)->size++] = data;
    return true;
}

void vec_cleanup(Vec_t *vec, void (*cleanup_callback)(void *data)) {
    if (vec == NULL) return;
    for (size_t i = 0; i < vec->size; i++) {
        cleanup_callback(vec->data[i]);
        vec->data[i] = NULL;
    }

    vec->size = 0;
}

void *vec_pop(Vec_t *vec) {
    if (vec == NULL) return NULL;
    return vec->data[vec->size--];
}

void vec_destroy(Vec_t *vec) { free(vec); }

#endif  //! USE_VECTORS
#endif  // UTIL_IMPL
#endif  // !UTIL_H
