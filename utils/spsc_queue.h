#pragma once
#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "utils.h"

#ifndef SPSC_Q_NO_SHORT_NAMES
#define q_init     spsc_q_init
#define q_push     spsc_q_push
#define q_front    spsc_q_front
#define q_pop      spsc_q_pop
#define q_pop_val  spsc_q_pop_val
#define q_is_empty spsc_q_is_empty
#define q_capacity spsc_q_capacity
#define q_free     spsc_q_free
#endif

#define CACHELINE 64

typedef struct SPSC_QueueHeader {
    size_t capacity;
    size_t mask;

    _Alignas(CACHELINE) _Atomic size_t head;
    _Alignas(CACHELINE) _Atomic size_t tail;
} SPSC_QueueHeader;

#define q_header(q) ((SPSC_QueueHeader *)(q) - 1)


#define spsc_q_init(q, cap) do {                             \
    size_t _cap = next_pow2(cap);                            \
    SPSC_QueueHeader *h = malloc(                            \
            sizeof(SPSC_QueueHeader) + sizeof(*(q)) * _cap   \
            );                                               \
    if (!h) { perror("malloc"); exit(1); }                   \
                                                             \
    h->capacity = _cap;                                      \
    h->mask = _cap - 1;                                      \
                                                             \
    atomic_store(&h->head, 0);                               \
    atomic_store(&h->tail, 0);                               \
                                                             \
    (q) = (void*)(h + 1);                                    \
} while (0)


// push (producer only)
#define spsc_q_push(q, val) ({                                           \
    int _ok = 1;                                                         \
    if (!(q)) q_init((q), 1024);                                         \
                                                                         \
    SPSC_QueueHeader *h = q_header(q);                                   \
                                                                         \
    size_t tail = atomic_load_explicit(&h->tail, memory_order_relaxed);  \
    size_t head = atomic_load_explicit(&h->head, memory_order_acquire);  \
                                                                         \
    if ((tail - head) == h->capacity) {                                  \
        _ok = 0;                                                         \
    } else {                                                             \
        (q)[tail & h->mask] = (val);                                     \
        atomic_store_explicit(&h->tail, tail + 1, memory_order_release); \
    }                                                                    \
    _ok;                                                                 \
})

// Returns front element (consumer only)
#define spsc_q_front(q) ({                                                  \
    __auto_type _q = (q);                                                   \
    __auto_type _res = (__typeof__(*(_q))) {0};                             \
                                                                            \
    if (_q) {                                                               \
        SPSC_QueueHeader *h = q_header(_q);                                 \
                                                                            \
        size_t head = atomic_load_explicit(&h->head, memory_order_relaxed); \
        size_t tail = atomic_load_explicit(&h->tail, memory_order_acquire); \
                                                                            \
        if (head != tail) {                                                 \
            _res = _q[head & h->mask];                                      \
        }                                                                   \
    }                                                                       \
                                                                            \
    _res;                                                                   \
})

// Just pops an element, without returning
#define spsc_q_pop(q) do {                                                   \
    if (q) {                                                                 \
        SPSC_QueueHeader *h = q_header(q);                                   \
                                                                             \
        size_t head = atomic_load_explicit(&h->head, memory_order_relaxed);  \
        size_t tail = atomic_load_explicit(&h->tail, memory_order_acquire);  \
                                                                             \
        if (head != tail) {                                                  \
            atomic_store_explicit(&h->head, head + 1, memory_order_release); \
        }                                                                    \
    }                                                                        \
} while (0)


// (Consumer only) Pops an element and returns it, or 0 if the queue is empty.
// Note that 0 is a valid value, so the caller should check
// if the queue is empty before calling this macro to distinguish
// between an empty queue and a queue with 0 as the front element.
#define spsc_q_pop_val(q) ({                                                 \
    __auto_type _q = (q);                                                    \
    __auto_type _res = (_q)[0]; /* deduce type */                            \
                                                                             \
    if (!_q) {                                                               \
        _res = (__typeof__(_res))0;                                          \
    } else {                                                                 \
        SPSC_QueueHeader *h = q_header(_q);                                  \
                                                                             \
        size_t head = atomic_load_explicit(&h->head, memory_order_relaxed);  \
        size_t tail = atomic_load_explicit(&h->tail, memory_order_acquire);  \
                                                                             \
        if (head != tail) {                                                  \
            _res = _q[head & h->mask];                                       \
            atomic_store_explicit(&h->head, head + 1, memory_order_release); \
        } else {                                                             \
            _res = (__typeof__(_res))0;                                      \
        }                                                                    \
    }                                                                        \
    _res;                                                                    \
})


// --- helpers ---

#define spsc_q_is_empty(q) ((q) ?                                           \
    (atomic_load_explicit(&q_header(q)->head, memory_order_relaxed) == \
     atomic_load_explicit(&q_header(q)->tail, memory_order_relaxed))   \
    : 1)

#define spsc_q_capacity(q) ((q) ? q_header(q)->capacity : 0)

#define spsc_q_free(q) do {            \
    if (q) {                           \
        free(q_header(q));             \
        (q) = NULL;                    \
    }                                  \
} while (0)
