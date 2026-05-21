#include "event_loop.h"
#include "utils.h"

#if defined(__linux__)

EventLoop *el_create(void)
{
    EventLoop *loop = malloc(sizeof(EventLoop));
    loop->event_loop_fd = epoll_create1(0);

    if (loop->event_loop_fd < 0)
    {
        free(loop);
        return NULL;
    }
    return loop;
}

void el_destroy(EventLoop *loop)
{
    close(loop->event_loop_fd);
    free(loop);
}

EL_Wakeup el_create_wakeup(void)
{
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd < 0)
    {
        return (EL_Wakeup){ .read_fd = -1, .write_fd = -1 };
    }
    return (EL_Wakeup){ .read_fd = efd, .write_fd = efd };
}

EL_STATUS_CODE el_add(
    EventLoop *loop,
    int       fd,
    int       flags,
    void      *userdata)
{
    struct epoll_event ev;
    uint32_t epoll_flags = 0;

    if (flags & EL_READ)  epoll_flags |= EPOLLIN;
    if (flags & EL_WRITE) epoll_flags |= EPOLLOUT;
    if (flags & EL_ET)    epoll_flags |= EPOLLET;

    ev.events   = epoll_flags;
    ev.data.ptr = userdata;

    if (epoll_ctl(loop->event_loop_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
    {
        return EL_ADD_FAIL;
    }
    return EL_OK;
}

EL_STATUS_CODE el_mod(
    EventLoop *loop,
    int       fd,
    int       flags,
    void      *userdata)
{
    struct epoll_event ev;
    uint32_t epoll_flags = 0;

    if (flags & EL_READ)  epoll_flags |= EPOLLIN;
    if (flags & EL_WRITE) epoll_flags |= EPOLLOUT;
    if (flags & EL_ET)    epoll_flags |= EPOLLET;

    ev.events   = epoll_flags;
    ev.data.ptr = userdata;

    if (epoll_ctl(loop->event_loop_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
    {
        return EL_MOD_FAIL;
    }

    return EL_OK;
}

EL_STATUS_CODE el_del(EventLoop *loop, int fd)
{
    if (loop->event_loop_fd >= 0)
    {
        if (epoll_ctl(loop->event_loop_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
        {
            return EL_DEL_FAIL;
        }
    }
    return EL_OK;
}

int el_wait(
    EventLoop *loop,
    EL_Event  *events,
    int       max_events,
    int       timeout_ms)
{
    struct epoll_event ep_events[MAX_EVENTS];

    int ready = epoll_wait(
        loop->event_loop_fd,
        ep_events,
        max_events,
        timeout_ms
    );

    if (ready <= 0) return ready;

    for (int i = 0; i < ready; i++)
    {
        events[i].userdata = ep_events[i].data.ptr;

        events[i].flags = 0;

        if (ep_events[i].events & EPOLLIN)
            events[i].flags |= EL_EVENT_READ;

        if (ep_events[i].events & EPOLLOUT)
            events[i].flags |= EL_EVENT_WRITE;

        if (ep_events[i].events & EPOLLHUP)
            events[i].flags |= EL_EVENT_HUP;

        if (ep_events[i].events & EPOLLERR)
            events[i].flags |= EL_EVENT_ERR;
    }

    return ready;
}

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

#include <sys/event.h>

EventLoop *el_create(void)
{
    EventLoop *loop = malloc(sizeof(EventLoop));

    if (!loop)
        return NULL;

    loop->event_loop_fd = kqueue();

    if (loop->event_loop_fd < 0)
    {
        free(loop);
        return NULL;
    }

    return loop;
}

void el_destroy(EventLoop *loop)
{
    close(loop->event_loop_fd);
    free(loop);
}

EL_Wakeup el_create_wakeup(void)
{
    int fds[2];
    if (pipe(fds) == -1) return (EL_Wakeup){0};

    set_nonblocking(fds[0]);
    set_nonblocking(fds[1]);

    return (EL_Wakeup){ fds[0], fds[1] };
}

EL_STATUS_CODE el_add(
    EventLoop *loop,
    int fd,
    int flags,
    void *userdata)
{
    struct kevent kev[2];
    int n = 0;

    uint16_t kflags = EV_ADD;

    if (flags & EL_ET)
        kflags |= EV_CLEAR;

    if (flags & EL_READ)
    {
        EV_SET(&kev[n++], fd, EVFILT_READ, kflags, 0, 0, userdata);
    }

    if (flags & EL_WRITE)
    {
        EV_SET(&kev[n++], fd, EVFILT_WRITE, kflags, 0, 0, userdata);
    }

    if (kevent(
        loop->event_loop_fd,
        kev,
        n,
        NULL,
        0,
        NULL
    ) < 0)
    {
        return EL_ADD_FAIL;
    }

    return EL_OK;
}

EL_STATUS_CODE el_mod(
    EventLoop *loop,
    int fd,
    int flags,
    void *userdata)
{
    el_del(loop, fd);
    return el_add(loop, fd, flags, userdata);
}

EL_STATUS_CODE el_del(EventLoop *loop, int fd)
{
    struct kevent kev[2];

    EV_SET(&kev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

    EV_SET(&kev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    kevent(
        loop->event_loop_fd,
        kev,
        2,
        NULL,
        0,
        NULL
    );

    return EL_OK;
}

int el_wait(
    EventLoop *loop,
    EL_Event *events,
    int max_events,
    int timeout_ms)
{
    struct kevent kev[MAX_EVENTS];

    struct timespec ts;
    struct timespec *tsp = NULL;

    if (timeout_ms >= 0)
    {
        ts.tv_sec  = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        tsp = &ts;
    }

    int ready = kevent(
        loop->event_loop_fd,
        NULL,
        0,
        kev,
        MAX_EVENTS,
        tsp
    );

    if (ready <= 0)
        return ready;

    for (int i = 0; i < ready; i++)
    {
        //events[i].fd = (int)kev[i].ident;

        events[i].userdata = kev[i].udata;

        events[i].flags = 0;

        if (kev[i].filter == EVFILT_READ)
            events[i].flags |= EL_EVENT_READ;

        if (kev[i].filter == EVFILT_WRITE)
            events[i].flags |= EL_EVENT_WRITE;

        if (kev[i].flags & EV_EOF)
            events[i].flags |= EL_EVENT_HUP;

        if (kev[i].flags & EV_ERROR)
            events[i].flags |= EL_EVENT_ERR;
    }

    return ready;
}

#endif


