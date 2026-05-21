#pragma once

#include "event_loop.h"
#include "server.h"

#include "spsc_queue.h"

#include <stdatomic.h>
#include <unistd.h>

// Message struct for inter-worker communication
// with reference counting
struct Message {
    uint8_t      *data;
    _Atomic int  refcount;
    uint32_t     len;
    uint32_t     sender_id;   // For not echoing msg to sender
};

// Worker struct representing a worker thread
// Each worker has its own epoll instance and client list
struct Worker {
    pthread_t    thread;

    int          *client_fd_queue;
    ClientTLS    **clients;

    EventLoop    *loop;
    EL_Wakeup    wakeup;

    int          id;
};

// Function to pass into a thread for running a worker
void *run_worker(void *arg);


static inline void init_worker(Worker *worker)
{
    worker->client_fd_queue = NULL;
    da_init(worker->clients, 256);
}


static inline void run_worker_thread(Worker *worker)
{
    if (pthread_create(&worker->thread, NULL, run_worker, worker) != 0)
    {
        ERROR("Failed to create worker thread");
    }
}


static inline void send_fd_to_worker(Worker *w, int client_fd)
{
    q_push(w->client_fd_queue, client_fd);

    uint64_t one = 1;
    write(w->wakeup.write_fd, &one, sizeof(one));
}


static inline void send_msg_to_workers(Message *msg, int current_worker_id)
{
    for (int wi = 0; wi < workers_count; wi++)
    {
        q_push(msg_queues[current_worker_id][wi], msg);

        //DEBUG("Sent message to worker %d from worker %d\n",
        //    workers[wi].id, current_worker_id);

        uint64_t one = 1;
        write(workers[wi].wakeup.write_fd, &one, sizeof(one));
    }
}


void *run_worker(void *arg)
{
    Worker *current_worker = (Worker *)arg;

    EL_Event events[MAX_EVENTS];

    while (1)
    {
        int ready = el_wait(
            current_worker->loop,
            events,
            MAX_EVENTS,
            -1
        );

        for (int ev_idx = 0; ev_idx < ready; ev_idx++)
        {
            // Check if it's a notification from the main thread
            if(events[ev_idx].userdata == &current_worker->wakeup)
            {
                uint64_t val;
                read(current_worker->wakeup.read_fd, &val, sizeof(val));

                while (!q_is_empty(current_worker->client_fd_queue))
                {
                    int fd = q_pop_val(current_worker->client_fd_queue);

                    if (add_client(current_worker, fd) != OK) {
                        ERROR("Failed to add client");
                    }
                }

                for (int i = 0; i < workers_count; i++)
                {
                    if (i == current_worker->id) continue;

                    Message **q = msg_queues[i][current_worker->id];

                    while (!q_is_empty(q))
                    {
                        Message *msg = q_pop_val(q);

                        broadcast_message(current_worker->clients, msg,
                                current_worker->loop);

                        if (atomic_fetch_sub(&msg->refcount, 1) == 1)
                        {
                            free(msg);
                        }
                    }
                }

                continue;
            }

            // CLIENT EVENT
            ClientTLS *c = events[ev_idx].userdata;

            // Check for hangup or error
            if (events[ev_idx].flags & EL_EVENT_HUP ||
                    events[ev_idx].flags & EL_EVENT_ERR)
            {
                mark_client_for_close(c);
                continue;
            }

            if (c->flags.state == HANDSHAKING)
            {
                if
                    (handle_handshake(current_worker->loop, c) != OK)
                    mark_client_for_close(c);
                continue;
            }

            // READ
            if (events[ev_idx].flags & EL_READ)
            {
                SERVER_STATUS err = handle_recv(
                    current_worker->clients,
                    c,
                    current_worker->loop,
                    current_worker->id
                );
                if (err < 0)
                {
                    mark_client_for_close(c);
                }
            }

            // WRITE
            if (events[ev_idx].flags & EL_WRITE)
            {
                SERVER_STATUS err = flush_send(c, current_worker);

                if (err != OK) {
                    mark_client_for_close(c);
                    continue;
                }

                // If buffer is empty — disable EPOLLOUT
                if (c->out_len == 0)
                {
                    if (el_mod(
                        current_worker->loop,
                        c->socket,
                        EL_READ | EL_ET,
                        c) < 0)
                    {
                        ERROR("Disabling EPOLLOUT failed");
                    }
                }

            }
        }

        for (size_t ci = 0; ci < da_get_size(current_worker->clients); )
        {
            ClientTLS *c = current_worker->clients[ci];

            if (c->flags.closing)
            {
                if (unlikely(remove_client(
                        current_worker->clients,
                        c,
                        current_worker->loop) != OK))
                {
                    ERROR("Failed to remove client");
                }
                continue;
            }

            ci++;
        }
    }

    return NULL;
}
