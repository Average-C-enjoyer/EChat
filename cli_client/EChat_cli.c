#include "client_core.h"
#include "menu.h"
#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define IP_LENGTH 15

// Debug mode in Visual Studio don't support argv
// so I need this macro to run correctly
//#define RUN_FROM_VS_DEBUG

#ifdef RUN_FROM_VS_DEBUG
    #define MAIN_ARGS void

    #define handle_main_args() do { \
        strcpy(ip, "127.0.0.1");    \
    } while (0)
#else
    #define MAIN_ARGS int argc, char *argv[]

    #define handle_main_args() do {                                  \
        if (argc > 1 && strcmp(argv[1], "--help") == 0) {            \
            printf("Using ip %s\n", argv[0]);                        \
            printf("A simple TLS chat client.\n");                   \
            printf("Options:\n");                                    \
            printf("  --help    Show this help message and exit\n"); \
            return 0;                                                \
        }                                                            \
        if (argc > 1) {                                              \
            strncpy(ip, argv[1], IP_LENGTH - 1);                     \
            ip[IP_LENGTH - 1] = '\0';                                \
        }                                                            \
        else {                                                       \
            strcpy(ip, "127.0.0.1");                                 \
        }                                                            \
    } while (0)
#endif

void on_connect(_Bool connected)
{
    if (connected) {
        printf("Connected succesfully!\n");
    } else {
        printf("Unable to connect!\n");
    }
}

void on_message(const char *msg, uint32_t len)
{
    printf("%.*s\n", (int)len, msg);
}

void on_error(uint32_t error_code, const char *error_message)
{
    printf("[ERROR] %s: %d\n", error_message, error_code);
}



int main(MAIN_ARGS) {
    char ip[IP_LENGTH];

    handle_main_args();

    // START MENU
    terminal_init();

    MenuButtons menu;

    init_menu(&menu);

    while (1) {
        if (menu.type == MAIN_MENU) {
            display_menu(&menu, 1, 1);
            handle_menu_selection(&menu,1, 1);
        }
        else if (menu.type == CHAT_SELECTION_MENU) {
            display_menu(&menu, 0, 0);
            handle_menu_selection(&menu,0, 0);
        }
        else {
            break;
        }
    }
    terminal_restore();

#ifndef _WIN32
    system("stty sane");
#endif
    // Creating handle
    client_handle_t handle = client_create();

    client_set_on_connect_callback(handle, on_connect);
    client_set_on_message_callback(handle, on_message);
    client_set_on_error_callback(handle, on_error);

    printf("Connecting...\n");
    client_connect(handle, (uint8_t*)ip);
    printf("Connected\n");
    // Name input
    {
        char name[MAX_NAME_LENGTH];
        while (1)
        {
            printf("Enter name (16 characters max): ");
            scanf("%16s", name);

            size_t len = strlen(name);

            if (len && name[len - 1] == '\n')
                name[len - 1] = '\0';

            if (strchr(name, ' '))
            {
                printf("Invalid name: no spaces allowed\n");
                name[0] = '\0';
                continue;
            }
            break;
        }

        client_set_name(handle, name);
    }

    CLEAR_SCREEN();

    printf(BG_GREEN"Enter messages:\n"RESET_COLOR);

    uint8_t msg[4096];
    size_t cap = 0;

    while (TRUE)
    {
        // 4092 = MAX_MAX_MESSAGE_SIZE - 1 byte for \0 and 4 bytes length
        scanf("%4092s", msg);

        if (strcmp((char *)msg, ":exit") == 0)
        {
            exit(0);
        }

        client_send(handle, msg);
    }

    client_disconnect(handle);
    client_destroy(handle);

    return 0;
}
