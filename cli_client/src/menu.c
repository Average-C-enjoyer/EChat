#include "terminal.h"
#include "menu.h"

#include <stdlib.h>

#define BIG_LOGO_WIDTH 40
#define SMALL_LOGO_WIDTH 19

// ================================================================
// Chat logo assets
// ================================================================

/* Array of lines so i can print and center it */

const char *big_logo[] = {
GREEN
    "████████  ███████ ██                ██",
"██▓      ▓██      ██        ████    ██",
"█▓▓▓░▓   ▓█▓      █▓▓▓██   █▓  ██ ███▓▓█▓",
"▓▓░      ░▓▓      █▓   ▓▓  █▓▓█▓▓   ▓▓░",
"░░▓░░▓░░  ░▓░░▓░░ ▓░   ░░  ▓░  ░░    ▓░░"
    RESET_COLOR
};


const char *small_logo[] = {
GREEN
    "██▀ ▄▀▀ █▄█ ▄▀▄ ▀█▀",
"█▄▄ ▀▄▄ █ █ █▀█  █ "
RESET_COLOR
};

// ================================================================
// Main menu buttons assets
// ================================================================
const char *btn_reg[] = {
"╔────────────────────╗",
"│ SIGN IN OR SIGN UP │",
"╚────────────────────╝"
};

const char *btn_reg_choosed[] = {
GREEN
    "╔────────────────────╗",
"│ SIGN IN OR SIGN UP │",
"╚────────────────────╝"
RESET_COLOR
};

const char *btn_guest[] = {
"╔────────────────────╗",
"│ CONTINUE AS GUEST  │",
"╚────────────────────╝"
};

const char *btn_guest_choosed[] = {
GREEN
    "╔────────────────────╗",
"│ CONTINUE AS GUEST  │",
"╚────────────────────╝"
RESET_COLOR
};


// ================================================================
// Chat selection menu buttons assets
// ================================================================

const char *btn_enter_group_chat[] = {
"╔────────────────────╗",
"│  ENTER GROUP CHAT  │",
"╚────────────────────╝"
};

const char *btn_enter_group_chat_choosed[] = {
GREEN
    "╔────────────────────╗",
"│  ENTER GROUP CHAT  │",
"╚────────────────────╝"
RESET_COLOR
};

const char *btn_enter_p2p_chat[] = {
"╔────────────────────╗",
"│   ENTER P2P CHAT   │",
"╚────────────────────╝"
};

const char *btn_enter_p2p_chat_choosed[] = {
GREEN
    "╔────────────────────╗",
"│   ENTER P2P CHAT   │",
"╚────────────────────╝"
RESET_COLOR
};


// ================================================================
// Chat window assets
// ================================================================

const char *group_chat_header =
GREEN
"██▀ ▄▀▀ █▄█ ▄▀▄ ▀█▀╒══════════════════════════════════════════════════════════════════════════════════════════════╗\n"
"█▄▄ ▀▄▄ █ █ █▀█  █ │                                                                                              ║\n"
"╓──────────────────┴──────────────────────────────────────────────────────────────────────────────────────────────║\n"
RESET_COLOR;

const char *p2p_chat_header =
GREEN
"██▀ ▄▀▀ █▄█ ▄▀▄ ▀█▀╒══════════════════════════════════════════════════════════════════════════════════════════════╗\n"
"█▄▄ ▀▄▄ █ █ █▀█  █ │                                                                                              ║\n"
"╓──────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────║\n"
RESET_COLOR;


// ================================================================
// Utility funcs
// ================================================================

void draw_frame(int w, int h) {
    // Top border
    printf(GREEN"╔");
    for (int i = 0; i < w - 2; i++) printf("═");
    printf("╗");

    // Middle
    for (int y = 1; y < h - 1; y++) {
        printf("\n║");
        for (int i = 0; i < w - 2; i++) printf(" ");
        printf("║");
    }

    // Bottom
    printf("\n╚");
    for (int i = 0; i < w - 2; i++) printf("═");
    printf("╝"RESET_COLOR);
}

static inline void switch_active_button(MenuButtons *menu)
{
    switch (menu->type) {
    case MAIN_MENU:
        switch (menu->active_button) {
        case BUTTON_1:
            menu->button1 = btn_reg;
            menu->button2 = btn_guest_choosed;
            menu->active_button = BUTTON_2;
            break;
        case BUTTON_2:
            menu->button1 = btn_reg_choosed;
            menu->button2 = btn_guest;
            menu->active_button = BUTTON_1;
            break;
        }
        break;
    case CHAT_SELECTION_MENU:
        switch (menu->active_button) {
        case BUTTON_1:
            menu->button1 = btn_enter_group_chat;
            menu->button2 = btn_enter_p2p_chat_choosed;
            menu->active_button = BUTTON_2;
            break;
        case BUTTON_2:
            menu->button1 = btn_enter_group_chat_choosed;
            menu->button2 = btn_enter_p2p_chat;
            menu->active_button = BUTTON_1;
            break;
        }
        break;
    case ENTER_CHAT:
        break;
    }
}

static void go_to_chat(menu_handle_t handle)
{
    MenuButtons *menu = (MenuButtons *)handle;
    menu->type = ENTER_CHAT;
}

static void reg_pressed(menu_handle_t handle)
{
    MenuButtons *menu = (MenuButtons *)handle;

    menu->button1 = btn_enter_group_chat_choosed;
    menu->button2 = btn_enter_p2p_chat;
    menu->active_button = BUTTON_1;
    menu->type = CHAT_SELECTION_MENU;

    menu->btn1_pressed = go_to_chat;
    menu->btn2_pressed = go_to_chat;
}

static void guest_pressed(menu_handle_t handle)
{
    MenuButtons *menu = (MenuButtons *)handle;

    menu->button1 = btn_enter_group_chat_choosed;
    menu->button2 = btn_enter_p2p_chat;
    menu->active_button = BUTTON_1;
    menu->type = CHAT_SELECTION_MENU;

    menu->btn1_pressed = go_to_chat;
    menu->btn2_pressed = go_to_chat;
}

#ifdef _WIN32

void handle_menu_selection(StartMenu *menu)
{
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD record;
    DWORD events;

    // Disable line buffering & echo
    SetConsoleMode(hInput, ENABLE_EXTENDED_FLAGS);

    while (1) {
        ReadConsoleInput(hInput, &record, 1, &events);

        if (record.EventType == KEY_EVENT &&
            record.Event.KeyEvent.bKeyDown) {

            WORD key = record.Event.KeyEvent.wVirtualKeyCode;

            if (key == VK_UP || key == VK_DOWN) {
                switch_active_button(menu);
                terminal_clear_home();
                display_menu(menu, 1, 1);
            }
            else if (key == VK_RETURN) {
                if (menu->type == BUTTON_REG)
                    reg_pressed();
                else
                    guest_pressed();
            }
        }
    }
}

#else // Linux / POSIX

static struct termios global_orig;

static inline void enable_raw_mode(void)
{
    struct termios raw;

    tcgetattr(STDIN_FILENO, &global_orig);
    raw = global_orig;

    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    //atexit(tcsetattr(STDIN_FILENO, TCSAFLUSH, &global_orig));
}

static inline void restore_stdin_mode(void)
{
    struct termios t;

    tcgetattr(STDIN_FILENO, &t);

    t.c_lflag |= (ICANON | ECHO);  // return to normal mode

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

void handle_menu_selection(
        MenuButtons *menu,
        _Bool display_logo,
        _Bool display_hint)
{
    char c;

    enable_raw_mode();

    while (1) {
        if (read(STDIN_FILENO, &c, 1) != 1)
            continue;

        if (c == '\033') { // ESC
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[0] == '[' &&
                (seq[1] == 'A' || seq[1] == 'B')) {

                switch_active_button(menu);
                terminal_clear_home();
                display_menu(menu, display_logo, display_hint);
            }
        }
        else if (c == '\n' || c == '\r') {
            if (menu->active_button == BUTTON_1) {
                menu->btn1_pressed(menu);
            } else {
                menu->btn2_pressed(menu);
            }
           break;
        }
    }

    restore_stdin_mode();
}
#endif // _WIN32


// =========================================================================
// Public API
// =========================================================================

void init_menu(MenuButtons *menu) {
    menu->button1 = btn_reg_choosed;
    menu->button2 = btn_guest;
    menu->active_button = BUTTON_1;
    menu->type = MAIN_MENU;

    menu->btn1_pressed = reg_pressed;
    menu->btn2_pressed = guest_pressed;
}

void display_menu(MenuButtons *menu, _Bool display_logo, _Bool display_hint)
{
    TermSize ts = terminal_get_size();

    terminal_clear_home();

    draw_frame(ts.width, ts.height);

    int y = 3;

    if (display_logo) {
        // --- choose logo ---
        int use_tiny = ts.width < BIG_LOGO_WIDTH;

        const char **logo;
        int logo_h;
        int logo_w;

        if (use_tiny) {
            logo = small_logo;
            logo_h = 2;
            logo_w = 22;
        }
        else {
            logo = big_logo;
            logo_h = 5;
            logo_w = 48;
        }

        // --- print logo ---
        int start_x = center_x(ts.width, logo_w);

        for (int i = 0; i < logo_h; i++) {
            print_at(start_x, y + i, logo[i]);
        }

        y += logo_h + 3;
    }

    if (display_hint) {
        // --- hint ---
        const char *hint_line =
            "Use " GREEN "UP" RESET_COLOR "/" GREEN "DOWN" RESET_COLOR " to navigate, " GREEN "ENTER" RESET_COLOR " to select";

        print_at(center_x(ts.width, 40), y, hint_line);

        y += 2;
    }

    // --- buttons ---
    int btn_x = center_x(ts.width, 28);

    if (btn_x < 2) btn_x = 2;
    if (btn_x > ts.width - 28) btn_x = ts.width - 28;

    for (int i = 0; i < 3; i++) {
        print_at(btn_x, y + i, menu->button1[i]);
    }
    y += 3;

    for (int i = 0; i < 3; i++) {
        print_at(btn_x, y + i, menu->button2[i]);
    }

}
