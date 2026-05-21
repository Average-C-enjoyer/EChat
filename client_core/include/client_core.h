#pragma once

#include <stdint.h>

#define IP_LENGTH 15
#define MAX_NAME_LENGTH 17

#define TRUE  1
#define FALSE 0

// Dll stuff
#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef ECHAT_DLL_EXPORTS
        #define ECHAT_API __declspec(dllexport)
    #else
        #define ECHAT_API __declspec(dllimport)
    #endif
#else
    #ifdef ECHAT_DLL_EXPORTS
        #define ECHAT_API __attribute__((visibility("default")))
    #else
        #define ECHAT_API
    #endif
#endif

/* -------Callback typedefs-------*/
typedef void(*on_connect_callback)(_Bool connected);
typedef void(*on_message_callback)(const char *msg, uint32_t len);
typedef void(*on_error_callback)(uint32_t error_code, const char *error_message);

// Handle for client
typedef void *client_handle_t;

/* -------Api funcs-------*/

ECHAT_API client_handle_t client_create();
ECHAT_API void client_destroy(client_handle_t handle);

// Connects client handle to server
ECHAT_API void client_connect(client_handle_t handle, uint8_t *ip);
// Disconnects client from server
ECHAT_API void client_disconnect(client_handle_t handle);
// Sends payload
ECHAT_API void client_send(client_handle_t handle, uint8_t *payload);

/* -------Callback setters-------*/

// Sets the on_connect collback to client struct (called after connection)
ECHAT_API void client_set_on_connect_callback(client_handle_t handle, on_connect_callback cb);
// Sets the on_message collback to client struct (called after message recieved)
ECHAT_API void client_set_on_message_callback(client_handle_t handle, on_message_callback cb);
// Sets the on_error collback to client struct (called after an error occured)
ECHAT_API void client_set_on_error_callback(client_handle_t handle, on_error_callback cb);

/* ------- Client fields setters------- */

// Sets name to client struct
ECHAT_API void client_set_name(client_handle_t handle, char *name);
