#ifndef MENU_H
#define MENU_H

#include <stdio.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <termios.h>
	#include <unistd.h>
#endif

#define RESET_COLOR             "\033[0m"

#define RED                     "\x1B[31m"
#define GREEN                   "\x1B[32m"
#define YELLOW                  "\x1B[33m"
#define BLUE                    "\x1B[34m"
#define MAGENTA                 "\x1B[35m"
#define CYAN                    "\x1B[36m"
#define WHITE                   "\x1B[37m"
#define LIGHT_YELLOW            "\x1B[93m"

#define BG_GREEN                "\033[3;42;30m"
#define BG_YELLOW               "\033[3;43;30m"
#define BG_BLUE                 "\033[3;44;30m"
#define BG_LIGHT_MAGENTA_DIM    "\033[2;47;35m"
#define BG_LIGHT_MAG            "\033[3;47;35m"
#define BG_LIGHT_BLUE           "\033[3;104;30m"
#define BG_DARK_GRAY            "\033[3;100;30m"
#define BG_LIGHT_ENTA_BRIGHT    "\033[1;47;35m"

typedef enum {
   BUTTON_1,
   BUTTON_2
} ActiveMainMenuBtn;

typedef enum {
    MAIN_MENU,
    CHAT_SELECTION_MENU,
    ENTER_CHAT
} ActiveMenuType;

typedef struct MenuButtons MenuButtons;

typedef MenuButtons *menu_handle_t;

// Just basic signature for handling multiple menu types
// without creating 100500 funcs for each one or
// implementing polymorphism in c.
// had to remove readable button names, sorry :(
struct MenuButtons {
    const char        **button1;
    const char        **button2;

    void              (*btn1_pressed)(menu_handle_t self);
    void              (*btn2_pressed)(menu_handle_t self);

    ActiveMainMenuBtn active_button;
    ActiveMenuType    type;
};

// API
void init_menu(MenuButtons *menu);
void display_menu(MenuButtons *menu, _Bool display_logo, _Bool display_hint);
void handle_menu_selection(
    MenuButtons *menu,
    _Bool display_logo,
    _Bool display_hint
);

#endif // MENU_H
