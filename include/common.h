#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_BUFFER 20000
#define INITIAL_WINDOW_WIDTH 900
#define INITIAL_WINDOW_HEIGHT 650
#define TAB_HEIGHT 30
#define TAB_WIDTH 100
#define PLUS_TAB_WIDTH 40
#define CLOSE_BUTTON_SIZE 10
#define CLOSE_BUTTON_MARGIN 8
#define FONT_HEIGHT 20
#define FONT_NAME "fixed"

#define PROMPT "user@myterm$ "
#define CONTINUE_PROMPT "> "
#define WATCH_INTERVAL 2

#define MAX_HISTORY 10000
#define SHOW_HISTORY_COUNT 1000

/* Global variables for multiWatch signal handling */
extern volatile sig_atomic_t multiwatch_running;
extern pid_t *global_multiwatch_pids;
extern int global_multiwatch_count;
extern char **global_temp_files;

/* Signal handling - current foreground pid */
extern volatile pid_t current_fg_pid;

typedef struct {
    pid_t pid;
    char command[256];
    int job_id;
} BackgroundJob;

extern BackgroundJob background_jobs[100];
extern int bg_job_count;

typedef struct {
    char buffer[MAX_BUFFER];
    int buffer_len;
    char input[4096];
    int input_len;
    int cursor_pos;
    int continuation_mode;
    char full_command[4096];
    int full_command_len;
    pid_t shell_pid;
    int pipe_in[2];
    int pipe_out[2];
    int active;
    int scroll_offset;
    int user_scrolling;
    int search_mode;
    char search_input[512];
    int search_input_len;
    char **ac_matches;
    int ac_count;
    int ac_waiting_choice;
    char ac_prefix[512];
    int ac_prefix_len;
} Tab;

#ifndef WITHOUT_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

typedef struct {
    Display *display;
    Window window;
    GC gc;
    int screen;
    XFontStruct *font;
    Tab **tabs;
    int num_tabs;
    int current_tab;
    int tab_capacity;
    unsigned long grey_color;
    XIM xim;
    XIC xic;
    int window_width;
    int window_height;
} Terminal;
#else
typedef struct Terminal Terminal;
#endif

#endif /* COMMON_H */
