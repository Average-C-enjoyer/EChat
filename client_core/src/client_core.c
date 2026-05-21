#include "client_core.h"
#include "cross_platform_api.h"
#include "TLS.h"
#include <stdio.h>
#include <string.h>

#define DEFAULT_PORT "4433"
#define MAX_MESSAGE_SIZE 4096

// ================================================================
// Enums and structs
// ================================================================

// Enum for client error codes
typedef enum CLIENT_STATE {
    ERR_INIT_CREATE_CTX = -1,
    ERR_INIT_GAI        = -2,
    ERR_CONNECT         = -3,
    ERR_WINSOCK_INIT    = -4,
    ERR_BAD_LEN         = -5
} CLIENT_STATE;

// Error codes for TLS operations
typedef enum TLS_STATE {
    OK                  =  0,
    TLS_BAD_CONTEXT     = -6,
    TLS_BAD_CERT        = -7,
    TLS_BAD_KEY         = -8,
    SSL_ACCEPT_FAIL     = -9,
    SEND_FAIL           = -10,
    RECV_FAIL           = -11,
    BUFFER_OVERFLOW     = -12
} TLS_STATE;

// Client structure for TCP connections, TLS and callbacks
typedef struct ClientTLS_s {
    // TCP buffer
    uint8_t *in_buffer;

    uint8_t *name;

    // TLS
    SSL     *ssl;
    SSL_CTX *ctx;

    // recieve thread, starts automaticly.
    // I'm too lazy to implement a complite cross-platform
    // event loop only to read data, so i use multithreading
    // (better than basic select in my opinion)
    thread_t recv_thread;

    // Client callbacks
    void     (*on_connect) (_Bool connected);
    void     (*on_message) (const char *message, uint32_t len);
    void     (*on_error)   (uint32_t error_code, const char *error_message);

    socket_t socket;
    uint32_t in_len;

    volatile int running;   // Indicate if recv_thread is running
} ClientTLS;


// ================================================================
// Global variables
// ================================================================

// addrinfo for tcp connection
struct addrinfo  hints;


// ================================================================
// Private api
// ================================================================

static struct addrinfo *init_tcp(ClientTLS *client, uint8_t *ip)
{
    struct addrinfo *result = NULL;

    if (INIT_WINSOCK() != 0)
    {
        if (client->on_error)
        {
            client->on_error(
                ERR_WINSOCK_INIT, "Failed to initialize WinSock2"
            );
        }
        return NULL;
    }

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int gai = getaddrinfo(
        (const char *)ip,
        DEFAULT_PORT,
        &hints,
        &result
    );
    if (gai != 0)
    {
        if (client->on_error)
        {
            client->on_error(ERR_INIT_GAI, gai_strerror(gai));
        }
		return NULL;
    }

    return result;
}

static CLIENT_STATE init_tls(ClientTLS *client)
{
	init_openssl();

    client->ctx = create_ctx();
    if (!client->ctx) {
        if (client->on_error)
        {
            client->on_error(
                ERR_INIT_CREATE_CTX, "Failed to create SSL context"
            );
        }
        return ERR_INIT_CREATE_CTX;
    }
    return (CLIENT_STATE)OK;
}


static CLIENT_STATE connect_to_server(
    ClientTLS *client,
    struct addrinfo *result
)
{
    struct addrinfo *ptr = NULL;
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        client->socket = socket(
            ptr->ai_family,
            ptr->ai_socktype,
            ptr->ai_protocol
        );

        if (client->socket < 0)
        {
            continue;
        }

        if (connect(
            client->socket,
            ptr->ai_addr,
            (int)ptr->ai_addrlen) == 0)
        {
            break;
        }

        CLOSE(client->socket);
        client->socket = -1;
    }

    if (client->socket < 0)
    {
        if (client->on_error)
        {
            client->on_error(
                ERR_CONNECT, "Unable to connect to server"
            );
        }
        return ERR_CONNECT;
    }

    freeaddrinfo(result);
    return (CLIENT_STATE)OK;
}


static TLS_STATE tls_handshake(ClientTLS *client)
{
    client->ssl = SSL_new(client->ctx);
    SSL_set_fd(client->ssl, (int)client->socket);


    if (SSL_connect(client->ssl) <= 0)
    {
        if (client->on_error)
        {
            client->on_error(
                SSL_ACCEPT_FAIL, "TLS handshake failed"
            );
        }
        return SSL_ACCEPT_FAIL;
    }

    if (!verify_certificate(client->ssl))
    {
        if (client->on_error)
        {
            client->on_error(
                TLS_BAD_CERT, "Certificate verification failed"
            );
        }
        return TLS_BAD_CERT;
    }

    return OK;
}



// Length-prefixed protocol I/O

static TLS_STATE send_packet(
    ClientTLS *client,
    const uint8_t *data,
    uint32_t len
)
{
    if (len > MAX_MESSAGE_SIZE)
    {
        if (client->on_error)
        {
            client->on_error(
                BUFFER_OVERFLOW, "Message size exceeds maximum allowed"
            );
        }
        return BUFFER_OVERFLOW;
    }

    uint32_t net_len = htonl(len);

    if (SSL_write(client->ssl, &net_len, sizeof(net_len)) <= 0)
    {
        if (client->on_error)
        {
            client->on_error(SEND_FAIL, "Failed to send packet length");
        }
        return SEND_FAIL;
    }

    if (SSL_write(client->ssl, data, len) <= 0)
    {
        if (client->on_error)
        {
            client->on_error(
                SEND_FAIL, "Failed to send packet data"
            );
        }
        return SEND_FAIL;
    }

    return OK;
}

static TLS_STATE handle_recv(ClientTLS *client)
{
    if (client->running == FALSE)
    {
        pthread_exit(NULL);
        return OK;
    }

    int r = SSL_read(
        client->ssl,
        client->in_buffer + client->in_len,
        INPUT_BUFFER_SIZE - client->in_len
    );

    if (r <= 0)
    {
        if (client->on_error)
        {
            client->on_error(RECV_FAIL, "Failed to receive data");
        }
        return RECV_FAIL;
    }

    if (client->in_len > INPUT_BUFFER_SIZE) {
        client->on_error(BUFFER_OVERFLOW, "in_len corrupted");
        return BUFFER_OVERFLOW;
    }
    client->in_len += r;

    while (client->in_len >= 4)
    {
        uint32_t msg_len;
        memcpy(&msg_len, client->in_buffer, 4);
        msg_len = ntohl(msg_len);

        if (msg_len > MAX_MESSAGE_SIZE)
        {
            if (client->on_error)
            {
                client->on_error(
                    BUFFER_OVERFLOW,
                    "Received message size exceeds maximum allowed"
                );
            }
            return BUFFER_OVERFLOW;
        }

        if (client->in_len < 4 + msg_len)
        {
            break;
        }

        uint8_t *payload = client->in_buffer + 4;

        if (client->on_message)
        {
            client->on_message((char*)payload, msg_len);
        }

        memmove(
            client->in_buffer,
            client->in_buffer + 4 + msg_len,
            client->in_len - 4 - msg_len
        );

        client->in_len -= (4 + msg_len);
    }

    return OK;
}

// Send thread (main)
static void client_do_send(ClientTLS *client, uint8_t *buffer)
{
    size_t len = strlen((char*)buffer);

    if (len == 0)
    {
        if (client->on_error)
        {
            client->on_error(
                ERR_BAD_LEN, "Invalid buffer: length is equal to zero"
            );
            return;
        }
    }

    if (len && buffer[len - 1] == '\n')
    {
        buffer[--len] = '\0';
    }

    int8_t err = send_packet(client, (uint8_t *)buffer, (uint32_t)len);
    if (err < 0)
    {
        if (client->on_error)
        {
            client->on_error(err, "Failed to send packet");
        }
    }
}

// Recieve thread
static client_do_read_decl(client)
{
    if (client->running == FALSE)
    {
        pthread_exit(NULL);
        return NULL;
    }

    while (client->running) {
        int8_t err = handle_recv(client);

        if (err != OK) {
            if (client->running && client->on_error)
                client->on_error(err, "Receive failed");
            break;
        }
    }

    return NULL;
}


// ======================================================================
// Public API
// ======================================================================

/* ----------------------Create and destroy----------------------------*/
ECHAT_API client_handle_t client_create()
{
    ClientTLS *client = calloc(1, sizeof(ClientTLS));
    client->in_buffer = malloc(MAX_MESSAGE_SIZE);
    client->name = malloc(MAX_NAME_LENGTH);
    client->in_len = 0;
    client->running = FALSE;

    return (client_handle_t)client;
}

ECHAT_API void client_destroy(client_handle_t handle)
{
    ClientTLS *client = handle;

    free(client->in_buffer);
    free(client->name);
    free(client);
}

/* ---------------------------Connection-------------------------------*/
ECHAT_API void client_connect(client_handle_t handle, uint8_t *ip)
{
    ClientTLS *client = (ClientTLS *)handle;

    client->on_message("Initialize TCP...", 18);
    struct addrinfo *result = init_tcp(client, ip);
    if (result == NULL)
    {
        if (client->on_connect) client->on_connect(FALSE);
        return;
    }

    client->on_message("Initialize TLS...", 18);
    if (init_tls(client) < 0)
    {
        if (client->on_connect) client->on_connect(FALSE);
        return;
    }
    client->on_message("Connecting to server...", 24);
    if (connect_to_server(client, result) < 0)
    {
        if (client->on_connect) client->on_connect(FALSE);
        return;
    }

    client->on_message("Handshaking...", 15);
    if (tls_handshake(client) < 0)
    {
        if (client->on_connect) client->on_connect(FALSE);
        return;
    }
    client->on_message("Handshake done, starting read", 30);
    client->running = 1;

    THREAD_CREATE(client->recv_thread, client_do_read, client);

    if (client->on_connect) client->on_connect(TRUE);
}

ECHAT_API void client_disconnect(client_handle_t handle)
{
    ClientTLS *client = (ClientTLS *)handle;

    if (!client->running) return;

    client->running = FALSE;

    // Wake up SSL_read by shutting down the socket
    SHUTDOWN_SEND(client->socket);

    // Wait for receive thread to exit before freeing SSL resources
    THREAD_JOIN(client->recv_thread);

    // Now safe to free SSL resources (thread has exited)
    if (client->ssl) {
        SSL_free(client->ssl);
        client->ssl = NULL;
    }
    if (client->ctx) {
        SSL_CTX_free(client->ctx);
        client->ctx = NULL;
    }

    CLOSE(client->socket);

    WSA_CLEANUP();
}

/* -----------------------------Sending--------------------------------*/
ECHAT_API void client_send(client_handle_t handle, uint8_t *payload)
{
    ClientTLS *client = (ClientTLS *)handle;

    client_do_send(client, payload);
}

/* ------------------------Callback setters----------------------------*/
ECHAT_API void client_set_on_connect_callback(
    client_handle_t handle,
    on_connect_callback cb
)
{
    ClientTLS *client = (ClientTLS *)handle;

    if (cb) client->on_connect = cb;
}

ECHAT_API void client_set_on_message_callback(
    client_handle_t handle,
    on_message_callback cb
)
{
    ClientTLS *client = (ClientTLS *)handle;

    if (cb) client->on_message = cb;
}

ECHAT_API void client_set_on_error_callback(
    client_handle_t handle,
    on_error_callback cb
)
{
    ClientTLS *client = (ClientTLS *)handle;

    if (cb) client->on_error = cb;
}

/* ----------------------Client struct field setters ------------------*/
ECHAT_API void client_set_name(client_handle_t handle, char *name)
{
    ClientTLS *client = (ClientTLS *)handle;

    strncpy((char *)client->name, (char *)name, MAX_NAME_LENGTH - 1);
    client->name[MAX_NAME_LENGTH - 1] = '\0';
}
