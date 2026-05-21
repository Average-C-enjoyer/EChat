#pragma once

#include "d_array.h"
#include "event_loop.h"

#include <openssl/ssl.h>
#include <errno.h>

#if __STDC_VERSION__ < 201112L || __STDC_NO_ATOMICS__ == 1
#error "Atomics not supported. Cannot compile"
#endif

#define TRUE 1
#define FALSE 0

#define DEFAULT_PORT "4433"
#define INPUT_BUFFER_SIZE 4096
#define OUTPUT_BUFFER_SIZE 4096
#define LEN_PREFIX_SIZE 4



// Forward declarations
typedef struct ClientFlags ClientFlags;
typedef struct Worker Worker;
typedef struct ClientTLS ClientTLS;
typedef struct Message Message;

// ===============================
// Global variables
// ===============================

// Workers array and count
extern Worker *workers;
extern _Atomic(int)workers_count;

/*
Okay, this is complex...
Its a 2d array of queues of pointers to Message struct.
Each pair of worker threads has its own queue
for sending messages to each other.
like N*N queues for N workers. Each queue is SPSC

Indexing like:
msg_queues[i][j]

Where
i = sender (producer)
j = receiver (consumer)

In memory:
     to →  0     1     2     3
from ↓  +-----+-----+-----+-----+
     0  |  -  | Q01 | Q02 | Q03 |
     1  | Q10 |  -  | Q12 | Q13 |
     2  | Q20 | Q21 |  -  | Q23 |
     3  | Q30 | Q31 | Q32 |  -  |
*/
extern Message ****msg_queues;


// ================================
// Enums for error handling
// ================================

// Client state: whether it's still handshaking or fully connected
typedef enum CLIENT_STATE {
    HANDSHAKING,
    CONNECTED
} CLIENT_STATE;

// Universal status code
#define OK 1

// Initialization status codes for server setup,
// including socket creation and binding errors
typedef enum INIT_STATUS {
    GETADDRINFO_FAIL      = -11,
    SOCKET_CREATE_FAIL    = -12,
    SETSOCKOPT_FAIL       = -13,
    BIND_FAIL             = -14,
    LISTEN_FAIL           = -15
} INIT_STATUS;

typedef enum INIT_TLS_STATUS {
    TLS_BAD_CONTEXT       = -1,
    TLS_BAD_CERT          = -2,
    TLS_BAD_KEY           = -3
} INIT_TLS_STATUS;

// Server status codes for various operations,
// including TLS errors, client handling, and epoll control
typedef enum SERVER_STATUS {
    CLIENT_INIT_FAIL      = -4,
    CLIENT_HANDSHAKE_FAIL = -5,
    SEND_FAIL             = -6,
    RECV_FAIL             = -7,
    BUFFER_OVERFLOW       = -8,
    REMOVE_CLIENT_FAIL    = -9,
    EPOLL_CTL_FAIL        = -10
} SERVER_STATUS;


// Main server status codes for overall server startup
typedef enum SERVER_MAIN_STATUS {
    INIT_SERVER_FAIL      = -16,
    INIT_TLS_FAIL         = -17
} SERVER_MAIN_STATUS;


// ================================
// Structs and types
// ================================

// Flags for client state
// Field "closing" needs for not closing connection instantly on error
// but marking it for closing and removing after processing all events
struct ClientFlags {
    _Bool        state;       // HANDSHAKING or CONNECTED
    _Bool        closing;
};

// Struct representing a connected client, including SSL state, buffers, and flags
struct ClientTLS {
    SSL          *ssl;

    uint8_t      *in_buffer;
    size_t       in_len;

    uint8_t      *out_buffer;
    size_t       out_len;
    size_t       out_sent;    // Bytes already sent from out_buffer

    size_t       index;       // index in the clients array
    int          socket;
    uint32_t     id;          // Unique client ID
    ClientFlags  flags;
};


// ================================
// Function declarations
// ================================

// Initializes the TLS server context, loading certificates and keys
INIT_TLS_STATUS init_tls();

// Initialize socets, TCP and stuff
INIT_STATUS init_server(int *server_socket);

// Adds a new client, creats SSL object and setts up non-blocking socket
SERVER_STATUS add_client(Worker *w, int sock);

// Removes a client, cleaning up resources
// Returns OK on success, REMOVE_CLIENT_FAIL on failure
SERVER_STATUS remove_client(
    ClientTLS   **clients,
    ClientTLS   *c,
    EventLoop   *loop
);

// Func for TLS handshake
// Returns OK if handshake is complete or still in progress,
// CLIENT_HANDSHAKE_FAIL on failure
SERVER_STATUS handle_handshake(EventLoop *loop, ClientTLS *c);

// Func for broadcasting the message to all other clients
void broadcast_message(
    ClientTLS   **clients,
    Message     *msg,
    EventLoop   *loop
);

// Handles incoming data from a client,
// processing complete messages and returns it
SERVER_STATUS handle_recv(
    ClientTLS   **clients,
    ClientTLS   *c,
    EventLoop   *loop,
    int         current_worker_id
);

// Flushes the send buffer for a client, handling partial writes and SSL errors
SERVER_STATUS flush_send(ClientTLS *c, Worker *w);

// Main fanc to start server
SERVER_MAIN_STATUS server_run();
