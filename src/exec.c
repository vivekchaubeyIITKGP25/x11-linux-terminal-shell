#include "exec.h"
#include "io.h"
#include "history.h"
#include "ui.h"
#ifndef WITHOUT_X11
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <glob.h>
#include <dirent.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <errno.h>
#endif

/* --------- Pure logic helper functions (X11-independent for unit testing) --------- */

void longest_common_prefix(char **arr, int n, char *out, size_t out_size) {
    if (n <= 0) { out[0] = '\0'; return; }
    strncpy(out, arr[0], out_size-1);
    out[out_size-1] = '\0';
    for (int i = 1; i < n; ++i) {
        int j = 0;
        while (out[j] && arr[i][j] && out[j] == arr[i][j]) j++;
        out[j] = '\0';
        if (out[0] == '\0') break;
    }
}

int token_start_index(const Tab *tab) {
    int pos = tab->cursor_pos;
    if (pos > tab->input_len) pos = tab->input_len;
    int start = pos - 1;
    while (start >= 0 && tab->input[start] != ' ' && tab->input[start] != '\t') start--;
    return start + 1;
}

int parse_redirection(char *cmd_copy, char **input_file, char **output_file) {
    *input_file = NULL;
    *output_file = NULL;

    char *output_redirect_pos = strchr(cmd_copy, '>');
    if (output_redirect_pos != NULL) {
        *output_redirect_pos = '\0';
        output_redirect_pos++;
        while (*output_redirect_pos == ' ' || *output_redirect_pos == '\t') output_redirect_pos++;
        *output_file = output_redirect_pos;
        char *end = *output_file + strlen(*output_file) - 1;
        while (end > *output_file && (*end == ' ' || *end == '\t' || *end == '\n')) { *end = '\0'; end--; }
    }

    char *input_redirect_pos = strchr(cmd_copy, '<');
    if (input_redirect_pos != NULL) {
        *input_redirect_pos = '\0';
        input_redirect_pos++;
        while (*input_redirect_pos == ' ' || *input_redirect_pos == '\t') input_redirect_pos++;
        *input_file = input_redirect_pos;
        char *end = *input_file + strlen(*input_file) - 1;
        while (end > *input_file && (*end == ' ' || *end == '\t' || *end == '\n')) { *end = '\0'; end--; }
    }
    return 0;
}

int parse_arguments(char *cmd_copy, char *args[], int max_args) {
    int argc = 0;
    char *p = cmd_copy;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    while (*p && argc < max_args - 2) {
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p) break;
        if (*p == '"' || *p == '\'') {
            char quote = *p; p++;
            args[argc++] = p;
            while (*p && *p != quote) p++;
            if (*p == quote) { *p = '\0'; p++; }
        } else {
            args[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) { *p = '\0'; p++; }
        }
    }

    if (argc > 0 && strcmp(args[0], "echo") == 0) {
        for (int i = argc; i > 0; i--) args[i + 1] = args[i];
        args[1] = "-e";
        argc++;
    }
    args[argc] = NULL;
    return argc;
}

void cleanup_temp_files(void) {
    if (global_temp_files) {
        for (int i = 0; i < global_multiwatch_count; i++) {
            if (global_temp_files[i]) {
                unlink(global_temp_files[i]);
                free(global_temp_files[i]);
            }
        }
        free(global_temp_files);
        global_temp_files = NULL;
    }
}

/* --------- X11 Dependent execution and autocomplete functions --------- */
#ifndef WITHOUT_X11

void handle_autocomplete(Tab *tab, Terminal *term) {
    free_autocomplete(tab);

    int start = token_start_index(tab);
    int pos = tab->cursor_pos;
    if (start < 0) start = 0;
    if (pos < start) pos = start;

    int prefix_len = pos - start;
    if (prefix_len < 0) prefix_len = 0;
    if (prefix_len >= (int)sizeof(tab->ac_prefix)) prefix_len = (int)sizeof(tab->ac_prefix)-1;
    memcpy(tab->ac_prefix, tab->input + start, prefix_len);
    tab->ac_prefix[prefix_len] = '\0';
    tab->ac_prefix_len = prefix_len;

    DIR *d = opendir(".");
    if (!d) return;
    struct dirent *de;
    char **matches = NULL;
    int matches_count = 0;

    while ((de = readdir(d)) != NULL) {
        if (prefix_len == 0) continue;
        if (strncmp(de->d_name, tab->ac_prefix, prefix_len) == 0) {
            matches = realloc(matches, (matches_count + 1) * sizeof(char*));
            matches[matches_count] = strdup(de->d_name);
            matches_count++;
        }
    }
    closedir(d);

    if (matches_count == 0) {
        if (matches) free(matches);
        return;
    }

    if (matches_count == 1) {
        const char *m = matches[0];
        size_t remain = strlen(m) - (size_t)prefix_len;
        if (remain > 0 && tab->input_len + (int)remain < (int)sizeof(tab->input) - 1) {
            memmove(tab->input + pos + remain, tab->input + pos, tab->input_len - pos + 1);
            memcpy(tab->input + pos, m + prefix_len, remain);
            tab->input_len += (int)remain;
            tab->cursor_pos += (int)remain;
        }
        free(matches[0]);
        free(matches);
        draw_terminal(term);
        return;
    }

    char lcp[512];
    longest_common_prefix(matches, matches_count, lcp, sizeof(lcp));
    int lcp_len = (int)strlen(lcp);

    if (lcp_len > prefix_len) {
        int extend = lcp_len - prefix_len;
        if (tab->input_len + extend < (int)sizeof(tab->input) - 1) {
            memmove(tab->input + pos + extend, tab->input + pos, tab->input_len - pos + 1);
            memcpy(tab->input + pos, lcp + prefix_len, extend);
            tab->input_len += extend;
            tab->cursor_pos += extend;
        }
        for (int i = 0; i < matches_count; ++i) free(matches[i]);
        free(matches);
        draw_terminal(term);
        return;
    }

    add_to_buffer(tab, "\n");
    for (int i = 0; i < matches_count; ++i) {
        char line[1024];
        snprintf(line, sizeof(line), "%d. %s\n", i + 1, matches[i]);
        add_to_buffer(tab, line);
    }
    add_to_buffer(tab, "Enter choice (1-N) : ");
    draw_terminal(term);

    tab->ac_matches = matches;
    tab->ac_count = matches_count;
    tab->ac_waiting_choice = 1;
}

void execute_external_command(Terminal *term, const char *command) {
    Tab *tab = term->tabs[term->current_tab];

    char cmd_copy[4096];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char *input_file = NULL;
    char *output_file = NULL;
    parse_redirection(cmd_copy, &input_file, &output_file);

    int pipefd[2];
    int use_pipe = (output_file == NULL);

    if (use_pipe && pipe(pipefd) == -1) {
        add_to_buffer(tab, "Error: Failed to create pipe\n");
        add_to_buffer(tab, PROMPT);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        add_to_buffer(tab, "Error: Fork failed\n");
        add_to_buffer(tab, PROMPT);
        if (use_pipe) { close(pipefd[0]); close(pipefd[1]); }
        return;
    }

    if (pid == 0) {
        /* ---- CHILD ---- */
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        if (use_pipe) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
        }

        if (output_file != NULL) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) { perror("open"); exit(1); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (input_file != NULL) {
            int fd = open(input_file, O_RDONLY);
            if (fd == -1) { perror("open"); exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        char *args[64];
        int argc = parse_arguments(cmd_copy, args, 64);

        if (argc > 0) {
            glob_t glob_result;
            glob_result.gl_offs = 0;
            int ret = 0;
            char **new_argv = malloc(sizeof(char*) * 1024);
            int new_argc = 0;
            new_argv[new_argc++] = args[0];

            for (int i = 1; i < argc; i++) {
                if (strpbrk(args[i], "*?[")) {
                    ret = glob(args[i], GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_result);
                    if (ret == 0) {
                        for (size_t j = 0; j < glob_result.gl_pathc; j++) {
                            new_argv[new_argc++] = strdup(glob_result.gl_pathv[j]);
                        }
                        globfree(&glob_result);
                    } else {
                        new_argv[new_argc++] = args[i];
                    }
                } else {
                    new_argv[new_argc++] = args[i];
                }
            }

            new_argv[new_argc] = NULL;
            execvp(new_argv[0], new_argv);
            perror("execvp");
            free(new_argv);
        }
        exit(1);
    } else {
        /* ---- PARENT ---- */
        setpgid(pid, pid);
        current_fg_pid = pid;

        if (use_pipe) {
            close(pipefd[1]);
            int fd = pipefd[0];
            fcntl(fd, F_SETFL, O_NONBLOCK);

            struct pollfd pfd = { .fd = fd, .events = POLLIN };
            char output_buffer[4096];
            int status = 0;

            while (1) {
                int poll_res = poll(&pfd, 1, 100);
                if (poll_res > 0 && (pfd.revents & POLLIN)) {
                    ssize_t bytes = read(fd, output_buffer, sizeof(output_buffer) - 1);
                    if (bytes > 0) {
                        output_buffer[bytes] = '\0';
                        add_to_buffer(tab, output_buffer);
                        draw_terminal(term);
                    }
                }

                while (XPending(term->display)) {
                    XEvent ev;
                    XNextEvent(term->display, &ev);
                    if (ev.type == KeyPress) handle_keypress(term, &ev.xkey);
                    else if (ev.type == Expose && ev.xexpose.count == 0) draw_terminal(term);
                    else if (ev.type == ConfigureNotify) {
                        if (ev.xconfigure.width != term->window_width || ev.xconfigure.height != term->window_height) {
                            term->window_width = ev.xconfigure.width;
                            term->window_height = ev.xconfigure.height;
                            draw_terminal(term);
                        }
                    }
                }

                pid_t w = waitpid(pid, &status, WNOHANG);
                if (w == pid) break;
            }

            close(fd);
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg) - 1, "[Output redirected to: %s]\n", output_file);
            msg[sizeof(msg) - 1] = '\0';
            add_to_buffer(tab, msg);
            int status;
            waitpid(pid, &status, 0);
        }

        current_fg_pid = -1;
    }

    add_to_buffer(tab, PROMPT);
}

void execute_piped_command(Terminal *term, const char *command) {
    Tab *tab = term->tabs[term->current_tab];

    int pipe_count = 0;
    for (const char *p = command; *p; p++)
        if (*p == '|') pipe_count++;
    if (pipe_count == 0) {
        execute_external_command(term, command);
        return;
    }

    char cmd_copy[4096];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char *commands[10];
    int cmd_count = 0;
    char *token = strtok(cmd_copy, "|");
    while (token != NULL && cmd_count < 10) {
        while (*token == ' ' || *token == '\t') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end = '\0';
            end--;
        }
        commands[cmd_count++] = token;
        token = strtok(NULL, "|");
    }

    if (cmd_count < 2) {
        execute_external_command(term, command);
        return;
    }

    int pipes[9][2];
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            add_to_buffer(tab, "Error: Failed to create pipe\n");
            add_to_buffer(tab, PROMPT);
            return;
        }
    }

    int capture_pipe[2];
    if (pipe(capture_pipe) == -1) {
        add_to_buffer(tab, "Error: Failed to create output pipe\n");
        add_to_buffer(tab, PROMPT);
        return;
    }

    pid_t pids[cmd_count];

    for (int i = 0; i < cmd_count; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            add_to_buffer(tab, "Error: Fork failed\n");
            add_to_buffer(tab, PROMPT);
            return;
        }

        if (pids[i] == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            if (i > 0) dup2(pipes[i - 1][0], STDIN_FILENO);
            if (i < cmd_count - 1) dup2(pipes[i][1], STDOUT_FILENO);
            else dup2(capture_pipe[1], STDOUT_FILENO);

            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            close(capture_pipe[0]);
            close(capture_pipe[1]);

            char *args[64];
            int argc = 0;
            char *arg_token = strtok(commands[i], " \t\n");
            if (arg_token != NULL && strcmp(arg_token, "echo") == 0) {
                args[argc++] = arg_token;
                args[argc++] = "-e";
                arg_token = strtok(NULL, " \t\n");
            }
            while (arg_token != NULL && argc < 63) {
                int len = strlen(arg_token);
                if (len >= 2 &&
                    ((arg_token[0] == '"' && arg_token[len - 1] == '"') ||
                     (arg_token[0] == '\'' && arg_token[len - 1] == '\''))) {
                    arg_token[len - 1] = '\0';
                    arg_token++;
                }
                args[argc++] = arg_token;
                arg_token = strtok(NULL, " \t\n");
            }
            args[argc] = NULL;

            if (argc > 0) {
                glob_t glob_result;
                glob_result.gl_offs = 0;
                int new_argc = 0;
                char **new_argv = malloc(sizeof(char*) * 1024);
                new_argv[new_argc++] = args[0];

                for (int a = 1; args[a] != NULL; a++) {
                    if (strpbrk(args[a], "*?[") != NULL) {
                        if (glob(args[a], GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_result) == 0) {
                            for (size_t g = 0; g < glob_result.gl_pathc; g++)
                                new_argv[new_argc++] = strdup(glob_result.gl_pathv[g]);
                            globfree(&glob_result);
                        } else {
                            new_argv[new_argc++] = args[a];
                        }
                    } else {
                        new_argv[new_argc++] = args[a];
                    }
                }
                new_argv[new_argc] = NULL;

                execvp(new_argv[0], new_argv);
                perror("execvp");
                free(new_argv);
            }
            exit(1);
        }
    }

    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    close(capture_pipe[1]);

    char buf[4096];
    ssize_t n;
    while ((n = read(capture_pipe[0], buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        add_to_buffer(tab, buf);
        draw_terminal(term);
    }
    close(capture_pipe[0]);

    for (int i = 0; i < cmd_count; i++) {
        int status;
        waitpid(pids[i], &status, WUNTRACED);
    }

    add_to_buffer(tab, PROMPT);
}

void execute_multiwatch(Terminal *term, const char *command) {
    Tab *tab = term->tabs[term->current_tab];

    char cmd_copy[4096];
    strncpy(cmd_copy, command + 11, sizeof(cmd_copy)-1);
    cmd_copy[sizeof(cmd_copy)-1] = '\0';

    char *p = cmd_copy;
    while (*p && (*p == ' ' || *p == '\t')) p++;

    int has_brackets = 0;
    if (*p == '[') { has_brackets = 1; p++; }

    char *commands[10];
    int cmd_count = 0;
    while (*p && cmd_count < 10) {
        while (*p && (*p == ' ' || *p == '\t' || *p == ',')) p++;
        if (*p == ']') break;
        if (!*p) break;
        if (*p == '"') {
            p++;
            commands[cmd_count] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') { *p = '\0'; p++; }
            cmd_count++;
        } else if (!has_brackets) {
            commands[cmd_count] = p;
            while (*p && *p != ',') p++;
            if (*p == ',') { *p = '\0'; p++; }
            char *end = commands[cmd_count] + strlen(commands[cmd_count]) - 1;
            while (end > commands[cmd_count] && (*end == ' ' || *end == '\t')) { *end = '\0'; end--; }
            cmd_count++;
        } else {
            add_to_buffer(tab, "Error: Commands in bracket syntax must be quoted\n");
            add_to_buffer(tab, "Usage: multiWatch [\"cmd1\", \"cmd2\", \"cmd3\"]\n");
            add_to_buffer(tab, "   or: multiWatch cmd1, cmd2, cmd3\n");
            add_to_buffer(tab, PROMPT);
            return;
        }
    }

    if (cmd_count == 0) {
        add_to_buffer(tab, "Usage: multiWatch [\"cmd1\", \"cmd2\", \"cmd3\"]\n");
        add_to_buffer(tab, "   or: multiWatch cmd1, cmd2, cmd3\n");
        add_to_buffer(tab, PROMPT);
        return;
    }

    add_to_buffer(tab, "[multiWatch started - Press Ctrl+C to stop]\n\n");
    draw_terminal(term);

    struct sigaction sa, sa_old;
    sa.sa_handler = multiwatch_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &sa_old);

    multiwatch_running = 1;

    global_multiwatch_pids = malloc(cmd_count * sizeof(pid_t));
    global_temp_files = malloc(cmd_count * sizeof(char*));
    global_multiwatch_count = cmd_count;

    for (int i = 0; i < cmd_count; i++) {
        global_temp_files[i] = malloc(64);
        snprintf(global_temp_files[i], 64, ".temp.%d_%d.txt", getpid(), i);
        global_temp_files[i][63] = '\0';

        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            while (1) {
                int fd = open(global_temp_files[i], O_WRONLY | O_CREAT | O_TRUNC, 0600);
                if (fd == -1) exit(1);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);

                char *args[64];
                int argc = 0;
                char cmd_buf[256];
                strncpy(cmd_buf, commands[i], sizeof(cmd_buf)-1);
                cmd_buf[sizeof(cmd_buf)-1] = '\0';

                char *token = strtok(cmd_buf, " \t\n");
                while (token && argc < 63) {
                    args[argc++] = token;
                    token = strtok(NULL, " \t\n");
                }
                args[argc] = NULL;

                if (argc > 0) {
                    pid_t exec_pid = fork();
                    if (exec_pid == 0) {
                        execvp(args[0], args);
                        exit(1);
                    } else if (exec_pid > 0) {
                        waitpid(exec_pid, NULL, 0);
                    }
                }
                sleep(WATCH_INTERVAL);
            }
            exit(0);
        } else if (pid > 0) {
            global_multiwatch_pids[i] = pid;
        } else {
            add_to_buffer(tab, "Error: Fork failed\n");
            add_to_buffer(tab, PROMPT);
            cleanup_temp_files();
            free(global_multiwatch_pids);
            return;
        }
    }

    int *fds = malloc(cmd_count * sizeof(int));
    time_t *last_read = malloc(cmd_count * sizeof(time_t));
    off_t *last_size = malloc(cmd_count * sizeof(off_t));
    for (int i = 0; i < cmd_count; i++) { fds[i] = -1; last_read[i] = 0; last_size[i] = 0; }

    usleep(100000);

    while (multiwatch_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = 0;

        for (int i = 0; i < cmd_count; i++) {
            if (fds[i] != -1) { close(fds[i]); fds[i] = -1; }
            fds[i] = open(global_temp_files[i], O_RDONLY);
            if (fds[i] != -1) {
                int flags = fcntl(fds[i], F_GETFL, 0);
                fcntl(fds[i], F_SETFL, flags | O_NONBLOCK);
                FD_SET(fds[i], &readfds);
                if (fds[i] > max_fd) max_fd = fds[i];
            }
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        select(max_fd + 1, &readfds, NULL, NULL, &tv);

        time_t now = time(NULL);

        for (int i = 0; i < cmd_count; i++) {
            if (fds[i] == -1) continue;
            struct stat st;
            if (fstat(fds[i], &st) == 0) {
                if (st.st_size > 0 && (st.st_size != last_size[i] || now - last_read[i] >= WATCH_INTERVAL)) {
                    last_size[i] = st.st_size;
                    last_read[i] = now;

                    struct tm *tm_info = localtime(&now);
                    char time_str[64];
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

                    char header[512];
                    snprintf(header, sizeof(header)-1, "\"%s\" , %s :\n----------------------------------------------------\n", commands[i], time_str);
                    header[sizeof(header)-1] = '\0';
                    add_to_buffer(tab, header);

                    char output_buf[4096];
                    ssize_t bytes_read;
                    lseek(fds[i], 0, SEEK_SET);
                    while ((bytes_read = read(fds[i], output_buf, sizeof(output_buf) - 1)) > 0) {
                        output_buf[bytes_read] = '\0';
                        add_to_buffer(tab, output_buf);
                    }

                    add_to_buffer(tab, "----------------------------------------------------\n\n");
                    draw_terminal(term);
                }
            }
        }

        XEvent event;
        while (XPending(term->display)) {
            XNextEvent(term->display, &event);
            if (event.type == KeyPress) {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_c && event.xkey.state & ControlMask) {
                    multiwatch_running = 0;
                    break;
                }
            } else if (event.type == Expose && event.xexpose.count == 0) {
                draw_terminal(term);
            } else if (event.type == ConfigureNotify) {
                if (event.xconfigure.width != term->window_width || event.xconfigure.height != term->window_height) {
                    term->window_width = event.xconfigure.width;
                    term->window_height = event.xconfigure.height;
                    draw_terminal(term);
                }
            }
        }
    }

    add_to_buffer(tab, "\n[multiWatch stopped by Ctrl+C]\n");

    for (int i = 0; i < cmd_count; i++) if (fds[i] != -1) close(fds[i]);

    for (int i = 0; i < cmd_count; i++) {
        if (global_multiwatch_pids[i] > 0) {
            kill(global_multiwatch_pids[i], SIGTERM);
            waitpid(global_multiwatch_pids[i], NULL, 0);
        }
    }

    cleanup_temp_files();

    free(global_multiwatch_pids);
    free(fds);
    free(last_read);
    free(last_size);
    global_multiwatch_pids = NULL;
    global_multiwatch_count = 0;

    sigaction(SIGINT, &sa_old, NULL);

    add_to_buffer(tab, "[Cleanup completed]\n");
    add_to_buffer(tab, PROMPT);
    draw_terminal(term);
}

void process_command(Terminal *term, const char *command) {
    Tab *tab = term->tabs[term->current_tab];
    if (strlen(command) == 0) {
        add_to_buffer(tab, "\n");
        add_to_buffer(tab, PROMPT);
        return;
    }

    add_history(command);

    if (strcmp(command, "history") == 0) {
        int start = history_count - SHOW_HISTORY_COUNT;
        if (start < 0) start = 0;
        for (int i = history_count - 1; i >= start; --i) {
            if (command_history[i]) {
                char line[4096];
                snprintf(line, sizeof(line)-1, "%d: %s\n", i+1, command_history[i]);
                line[sizeof(line)-1] = '\0';
                add_to_buffer(tab, line);
            }
        }
        add_to_buffer(tab, PROMPT);
        return;
    }

    if (strcmp(command, "jobs") == 0) {
        if (bg_job_count == 0) add_to_buffer(tab, "No background jobs\n");
        else {
            for (int i = 0; i < bg_job_count; i++) {
                char job_info[256];
                snprintf(job_info, sizeof(job_info)-1, "[%d] %d %s\n", background_jobs[i].job_id, background_jobs[i].pid, background_jobs[i].command);
                job_info[sizeof(job_info)-1] = '\0';
                add_to_buffer(tab, job_info);
            }
        }
        add_to_buffer(tab, PROMPT);
        return;
    }

    if (strncmp(command, "fg", 2) == 0) {
        int job_id = 1;
        if (strlen(command) > 3) job_id = atoi(command + 3);
        if (job_id > 0 && job_id <= bg_job_count) {
            pid_t pid = background_jobs[job_id - 1].pid;
            current_fg_pid = pid;
            kill(pid, SIGCONT);
            char msg[128];
            snprintf(msg, sizeof(msg)-1, "[%d] Continuing %d\n", job_id, pid);
            msg[sizeof(msg)-1] = '\0';
            add_to_buffer(tab, msg);
            int status;
            waitpid(pid, &status, WUNTRACED);
            current_fg_pid = -1;
            for (int i = job_id - 1; i < bg_job_count - 1; i++) background_jobs[i] = background_jobs[i+1];
            bg_job_count--;
        } else {
            add_to_buffer(tab, "fg: no such job\n");
        }
        add_to_buffer(tab, PROMPT);
        return;
    }

    if (strncmp(command, "bg", 2) == 0) {
        int job_id = 1;
        if (strlen(command) > 3) job_id = atoi(command + 3);
        if (job_id > 0 && job_id <= bg_job_count) {
            pid_t pid = background_jobs[job_id - 1].pid;
            kill(pid, SIGCONT);
            char msg[128];
            snprintf(msg, sizeof(msg)-1, "[%d] %d continued in background\n", job_id, pid);
            msg[sizeof(msg)-1] = '\0';
            add_to_buffer(tab, msg);
        } else add_to_buffer(tab, "bg: no such job\n");
        add_to_buffer(tab, PROMPT);
        return;
    }

    if (strcmp(command, "clear") == 0) {
        memset(tab->buffer, 0, MAX_BUFFER);
        tab->buffer_len = 0;
        strncpy(tab->buffer, PROMPT, sizeof(tab->buffer)-1);
        tab->buffer_len = (int)strlen(PROMPT);
        return;
    }

    if (strcmp(command, "exit") == 0) {
        add_to_buffer(tab, "Use Ctrl+Q to quit\n");
        add_to_buffer(tab, PROMPT);
        return;
    }

    if (strncmp(command, "cd ", 3) == 0) {
        const char *path = command + 3;
        while (*path == ' ') path++;
        if (chdir(path) == 0) {
            char cwd[256];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                add_to_buffer(tab, "Changed to: ");
                add_to_buffer(tab, cwd);
                add_to_buffer(tab, "\n");
            }
        } else {
            add_to_buffer(tab, "cd: ");
            add_to_buffer(tab, path);
            add_to_buffer(tab, ": No such directory\n");
        }
        add_to_buffer(tab, PROMPT);
        return;
    }

    if (strcmp(command, "cd") == 0) {
        const char *home = getenv("HOME");
        if (home != NULL) (void)!chdir(home);
        add_to_buffer(tab, PROMPT);
        return;
    }

    if (strncmp(command, "multiWatch ", 11) == 0) {
        execute_multiwatch(term, command);
        return;
    }

    if (strchr(command, '|') != NULL) execute_piped_command(term, command);
    else execute_external_command(term, command);
}
#endif
