#pragma once

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>

#define ERROR(...) error_impl(__VA_ARGS__, strerror(errno))

static inline void error_impl(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, ": %s\n", strerror(errno));
}

#define INFO(...) info_impl(__VA_ARGS__)

static inline void info_impl(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[INFO] ");
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

// Macro for debug logging, can be enabled by defining DEBUG_IMPL
#ifdef DEBUG_IMPL
#define DEBUG(...) debug_impl(__VA_ARGS__)

static inline void debug_impl(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[DEBUG] ");
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
#else
#define DEBUG(...) do { } while(0)
#endif

// Macro to set a file descriptor to non-blocking mode
#define set_nonblocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)

// Macro to mark a client for closing by setting the closing flag
#define mark_client_for_close(c) ((c)->flags.closing = TRUE)

#define likely(x)   __builtin_expect(!!(x), TRUE)
#define unlikely(x) __builtin_expect(!!(x), FALSE)


static inline size_t next_pow2(size_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    #if __SIZEOF_SIZE_T__ == 8
        x |= x >> 32;
    #endif
    return x + 1;
}
