#include "io.h"

volatile sig_atomic_t multiwatch_running = 0;
pid_t *global_multiwatch_pids = NULL;
int global_multiwatch_count = 0;
char **global_temp_files = NULL;

volatile pid_t current_fg_pid = -1;

BackgroundJob background_jobs[100];
int bg_job_count = 0;

void sigint_handler(int sig) {
    (void)sig;
#ifndef _WIN32
    if (current_fg_pid > 0) {
        kill(current_fg_pid, SIGINT);
        current_fg_pid = -1;
    }
#endif
}

void sigtstp_handler(int sig) {
    (void)sig;
#ifndef _WIN32
    if (current_fg_pid > 0) {
        kill(current_fg_pid, SIGSTOP);
        if (bg_job_count < 100) {
            background_jobs[bg_job_count].pid = current_fg_pid;
            background_jobs[bg_job_count].job_id = bg_job_count + 1;
            snprintf(background_jobs[bg_job_count].command, sizeof(background_jobs[bg_job_count].command),
                     "Process %d", current_fg_pid);
            char msg[256];
            snprintf(msg, sizeof(msg), "\n[%d]+ %d Stopped\n", bg_job_count + 1, current_fg_pid);
            (void)!write(STDOUT_FILENO, msg, strlen(msg));

            bg_job_count++;
        }
        current_fg_pid = -1;
    }
#endif
}

void multiwatch_signal_handler(int sig) {
    if (sig == SIGINT) multiwatch_running = 0;
}

void init_tab(Tab *tab) {
    memset(tab->buffer, 0, MAX_BUFFER);
    tab->buffer_len = 0;
    memset(tab->input, 0, sizeof(tab->input));
    tab->input_len = 0;
    tab->cursor_pos = 0;
    tab->continuation_mode = 0;
    memset(tab->full_command, 0, sizeof(tab->full_command));
    tab->full_command_len = 0;
    tab->active = 1;
    tab->shell_pid = -1;
    tab->scroll_offset = 0;
    tab->user_scrolling = 0;
    tab->search_mode = 0;
    tab->search_input_len = 0;
    tab->search_input[0] = '\0';
    tab->ac_matches = NULL;
    tab->ac_count = 0;
    tab->ac_waiting_choice = 0;
    tab->ac_prefix[0] = '\0';
    tab->ac_prefix_len = 0;
    strncpy(tab->buffer, PROMPT, sizeof(tab->buffer)-1);
    tab->buffer_len = (int)strlen(PROMPT);
}

void free_autocomplete(Tab *tab) {
    if (!tab) return;
    if (tab->ac_matches) {
        for (int i = 0; i < tab->ac_count; ++i) free(tab->ac_matches[i]);
        free(tab->ac_matches);
        tab->ac_matches = NULL;
    }
    tab->ac_count = 0;
    tab->ac_waiting_choice = 0;
    tab->ac_prefix[0] = '\0';
    tab->ac_prefix_len = 0;
}

#ifndef WITHOUT_X11
void close_tab(Terminal *term, int tab_index) {
    if (term->num_tabs <= 1) return;
    free_autocomplete(term->tabs[tab_index]);
    free(term->tabs[tab_index]);
    for (int i = tab_index; i < term->num_tabs - 1; ++i) term->tabs[i] = term->tabs[i+1];
    term->num_tabs--;
    if (term->current_tab >= term->num_tabs) term->current_tab = term->num_tabs - 1;
    else if (term->current_tab > tab_index) term->current_tab--;
}

void add_new_tab(Terminal *term) {
    if (term->num_tabs >= term->tab_capacity) {
        term->tab_capacity = term->tab_capacity * 2;
        term->tabs = realloc(term->tabs, term->tab_capacity * sizeof(Tab*));
        if (term->tabs == NULL) {
            fprintf(stderr, "Failed to allocate memory for new tabs\n");
            return;
        }
    }
    term->tabs[term->num_tabs] = malloc(sizeof(Tab));
    init_tab(term->tabs[term->num_tabs]);
    term->current_tab = term->num_tabs;
    term->num_tabs++;
}
#endif

void trim_buffer(Tab *tab) {
    if (tab->buffer_len > (int)(MAX_BUFFER * 0.8)) {
        int keep_size = (int)(MAX_BUFFER * 0.6);
        int remove_size = tab->buffer_len - keep_size;
        char *cut_point = strchr(tab->buffer + remove_size, '\n');
        if (cut_point) {
            int actual_remove = (int)(cut_point - tab->buffer) + 1;
            memmove(tab->buffer, tab->buffer + actual_remove, tab->buffer_len - actual_remove + 1);
            tab->buffer_len -= actual_remove;
            if (tab->buffer_len < 0) tab->buffer_len = 0;
        } else {
            memmove(tab->buffer, tab->buffer + tab->buffer_len - keep_size, keep_size);
            tab->buffer[keep_size] = '\0';
            tab->buffer_len = keep_size;
        }
    }
}

void add_to_buffer(Tab *tab, const char *text) {
    int text_len = (int)strlen(text);
    if (text_len == 0) return;
    if (tab->buffer_len + text_len >= MAX_BUFFER - 1) trim_buffer(tab);
    if (tab->buffer_len + text_len < MAX_BUFFER - 1) {
        memcpy(tab->buffer + tab->buffer_len, text, text_len);
        tab->buffer_len += text_len;
        tab->buffer[tab->buffer_len] = '\0';
    }
}
