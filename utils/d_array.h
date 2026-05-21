#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>

#include "utils.h"

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

typedef enum DARRAY_STATUS{
    DA_OK,
    DA_ERR_NOMEM,
    DA_ERR_BAD_INDEX,
} DARRAY_STATUS;

typedef struct ArrayHeader {
    size_t  size;
    size_t  capacity;
    uint8_t err;
} ArrayHeader;

// stb-style header getter
#define da_header(a) ((ArrayHeader *)((char *)(a) - sizeof(ArrayHeader)))

#define da_get_size(a) ((a) ? da_header(a)->size : 0)

#define da_init(a, cap) do {                          \
    size_t _cap = next_pow2(cap);                     \
    ArrayHeader *h = malloc(                          \
            sizeof(ArrayHeader) + sizeof(*(a)) * _cap \
            );                                        \
    if (!h) {                                         \
        perror("Malloc fail during da_init");         \
        exit(1);                                      \
    };                                                \
                                                      \
    h->capacity = _cap;                               \
    h->size = 0;                                      \
    h->err = 0;                                       \
                                                      \
    (a) = (void*)(h+1);                               \
} while(0)

#define da_free(a) do {           \
    if ((a)) {                    \
        free(da_header(a));       \
        (a) = NULL;               \
    }                             \
} while(0)

// Internal grow macro
#define ral_array_grow_internal(a) do {                                      \
    size_t old_cap = (a) ? da_header(a)->capacity : 0;                       \
    size_t new_cap = old_cap ? old_cap + old_cap / 2 : 32;                   \
                                                                             \
    if (new_cap < old_cap) {                                                 \
        if (a) da_header(a)->err = DA_ERR_NOMEM;                             \
        break;                                                               \
    }                                                                        \
                                                                             \
    size_t total_size = sizeof(ArrayHeader) + new_cap * sizeof(*(a));        \
    void *raw;                                                               \
    if ((a) == NULL) {                                                       \
        raw = malloc(total_size);                                            \
        if (!raw) break;                                                     \
        ArrayHeader *h = (ArrayHeader *)raw;                                 \
        h->size = 0;                                                         \
        h->capacity = new_cap;                                               \
        h->err = DA_OK;                                                      \
    } else {                                                                 \
        raw = realloc(da_header(a), total_size);                             \
        if (!raw) {                                                          \
            da_header(a)->err = DA_ERR_NOMEM;                                \
            break;                                                           \
        }                                                                    \
    }                                                                        \
    (a) = (typeof(*(a)) *)((char *)raw + sizeof(ArrayHeader));               \
    da_header(a)->capacity = new_cap;                                        \
    da_header(a)->err = DA_OK;                                               \
} while(0)

/* Internal macro realloc for */
#define ral_ralloc_array_internal(a) do {                                     \
    size_t total_size = sizeof(ArrayHeader) + da_header(a)->capacity * sizeof(*(a)); \
    void *raw = realloc(da_header(a), total_size);                            \
    if (!raw) {                                                               \
        da_header(a)->err = DA_ERR_NOMEM;                                     \
        break;                                                                \
    }                                                                         \
    (a) = (typeof(*(a)) *)((char *)raw + sizeof(ArrayHeader));                \
    da_header(a)->err = DA_OK;                                                \
} while(0)

#define da_append(a, value) do {                          \
    if ((a) == NULL || da_header(a)->size >= da_header(a)->capacity) { \
        ral_array_grow_internal(a);                       \
        if ((a) == NULL) break;  /* malloc failed */      \
        if (da_header(a)->err) break;                     \
    }                                                     \
    (a)[da_header(a)->size++] = value;                    \
    da_header(a)->err = DA_OK;                            \
} while(0)

// Shrink realloced memory to real size
#define da_shrink_to_fit(a) do {                          \
    if ((a) == NULL) break;                               \
    if (da_header(a)->size == 0) {                        \
        free(da_header(a));                               \
        (a) = NULL;                                       \
        break;                                            \
    }                                                     \
    da_header(a)->capacity = da_header(a)->size;          \
    ral_ralloc_array_internal(a);                         \
    if (da_header(a)->err) break;                         \
    da_header(a)->err = DA_OK;                            \
} while(0)

#define da_delete_at(a, index) do {                         \
    if ((a) == NULL || index >= da_header(a)->size) {       \
        if (a) da_header(a)->err = DA_ERR_BAD_INDEX;        \
        break;                                              \
    }                                                       \
    size_t sz = da_header(a)->size;                         \
    if (index < sz - 1) {                                   \
        memmove(                                            \
            &(a)[index],                                    \
            &(a)[index + 1],                                \
            (sz - index - 1) * sizeof(*(a))                 \
        );                                                  \
    }                                                       \
    da_header(a)->size--;                                   \
    da_header(a)->err = DA_OK;                              \
} while(0)

#define da_insert_at(a, index, value) do {                \
    if ((a) == NULL || (index) > da_header(a)->size) {    \
        if (a) da_header(a)->err = DA_ERR_BAD_INDEX;      \
        break;                                            \
    }                                                     \
    if (da_header(a)->size >= da_header(a)->capacity) {   \
        ral_array_grow_internal(a);                       \
        if ((a) == NULL) break;                           \
        if (da_header(a)->err) break;                     \
    }                                                     \
    size_t sz = da_header(a)->size;                       \
    if (index < sz) {                                     \
        memmove(                                          \
            &(a)[index + 1],                              \
            &(a)[index],                                  \
            (sz - index) * sizeof(*(a))                   \
        );                                                \
    }                                                     \
    (a)[index] = (value);                                 \
    da_header(a)->size++;                                 \
    da_header(a)->err = DA_OK;                            \
} while(0)

// Fast remove and replace with last elem
#define da_swap_remove(a, index) do {                     \
    if ((a) == NULL || (index) >= da_header(a)->size) {   \
        if (a) da_header(a)->err = DA_ERR_BAD_INDEX;      \
        break;                                            \
    }                                                     \
    size_t last = da_header(a)->size - 1;                 \
    if ((index) != last) {                                \
        (a)[index] = (a)[last];                           \
    }                                                     \
    da_header(a)->size--;                                 \
    da_header(a)->err = DA_OK;                            \
} while(0)

// Fast pointer remove with index update
#define da_swap_remove_ptr(a, idx) do {                   \
    if ((a) == NULL || (idx) >= da_header(a)->size) {     \
        if (a) da_header(a)->err = DA_ERR_BAD_INDEX;      \
        break;                                            \
    }                                                     \
    size_t last = da_header(a)->size - 1;                 \
    if ((idx) != last) {                                  \
        (a)[idx] = (a)[last];                             \
        if ((a)[idx]) (a)[idx]->index = idx;              \
    }                                                     \
    da_header(a)->size--;                                 \
    da_header(a)->err = DA_OK;                            \
} while(0)

// Get last error code
#define da_get_last_err(a) ((a) ? (DARRAY_STATUS)da_header(a)->err : DA_OK)

static inline void da_print_error(DARRAY_STATUS err) {
    switch (err) {
        case DA_OK:
            fprintf(stderr, "d_array: OK\n"); break;
        case DA_ERR_NOMEM:
            fprintf(stderr, "d_array: Out of memory\n"); break;
        case DA_ERR_BAD_INDEX:
            fprintf(stderr, "d_array: Bad index\n"); break;
        default:
            fprintf(stderr, "d_array: Unknown error\n");
    }
}

// Обработать ошибку (вывести, если не ОК)
#define da_handle_error(a) do { \
    DARRAY_STATUS _e = da_get_last_err(a); \
    if (_e != DA_OK) da_print_error(_e);   \
} while(0)
