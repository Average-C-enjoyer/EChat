#pragma once

#include <stdlib.h>
#include <unistd.h>

#define MAX_EVENTS 1024

typedef enum EL_STATUS_CODE {
    EL_OK        =  0,
    EL_ADD_FAIL  = -1,
    EL_DEL_FAIL  = -2,
    EL_MOD_FAIL  = -3,
    EL_WAIT_FAIL = -4,
} EL_STATUS_CODE;

// Input flags to pass into add/del/mod funcs
typedef enum {
    EL_READ  = 1 << 0,
    EL_WRITE = 1 << 1,
    EL_ET    = 1 << 2, // Edge-triggered
} EL_REGISTER_FLAGS;

// Flags for .flags field in EL_Event struct
// el_wait() automaticly fill this field with right flags.
// its like occured events on fd
typedef enum {
    EL_EVENT_READ  = 1 << 0,
    EL_EVENT_WRITE = 1 << 1,
    EL_EVENT_ERR   = 1 << 2,
    EL_EVENT_HUP   = 1 << 3,
} EL_EVENT_FLAGS;


typedef struct EventLoop {
    int event_loop_fd;
}EventLoop;

typedef struct {
    int       flags;
    void      *userdata;
} EL_Event;

typedef struct {
    int read_fd;
    int write_fd;
} EL_Wakeup;


EventLoop *el_create(void);

void el_destroy(EventLoop *loop);

EL_Wakeup el_create_wakeup(void);

EL_STATUS_CODE el_add(
    EventLoop *loop,
    int       fd,
    int       flags,
    void      *userdata
);

EL_STATUS_CODE el_mod(
    EventLoop *loop,
    int       fd,
    int       flags,
    void      *userdata
);

EL_STATUS_CODE el_del(
    EventLoop *loop,
    int       fd
);

int el_wait(
    EventLoop *loop,
    EL_Event  *events,
    int       max_events,
    int       timeout_ms
);
