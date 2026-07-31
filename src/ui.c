#include "ui.h"
#include "io.h"
#include "history.h"
#include "exec.h"

#ifndef WITHOUT_X11

void draw_terminal(Terminal *term) {
    XClearWindow(term->display, term->window);

    int visible_tabs = (term->window_width - PLUS_TAB_WIDTH) / TAB_WIDTH;
    if (visible_tabs <= 0) visible_tabs = 1;
    int start_tab = 0;
    if (term->num_tabs > visible_tabs) {
        start_tab = term->current_tab - visible_tabs / 2;
        if (start_tab < 0) start_tab = 0;
        if (start_tab + visible_tabs > term->num_tabs) start_tab = term->num_tabs - visible_tabs;
    }

    int display_idx = 0;
    for (int i = start_tab; i < term->num_tabs && display_idx < visible_tabs; i++) {
        int x = display_idx * TAB_WIDTH;
        if (i == term->current_tab) {
            XSetForeground(term->display, term->gc, 0x4444AA);
            XFillRectangle(term->display, term->window, term->gc, x, 0, TAB_WIDTH, TAB_HEIGHT);
            XSetForeground(term->display, term->gc, 0xFFFFFF);
        } else {
            XSetForeground(term->display, term->gc, 0x333333);
            XFillRectangle(term->display, term->window, term->gc, x, 0, TAB_WIDTH, TAB_HEIGHT);
            XSetForeground(term->display, term->gc, 0xAAAAAA);
        }
        char tab_label[20];
        snprintf(tab_label, sizeof(tab_label), "Tab %d", i + 1);
        XDrawString(term->display, term->window, term->gc, x + 10, 20, tab_label, (int)strlen(tab_label));

        if (term->num_tabs > 1) {
            int close_x = x + TAB_WIDTH - CLOSE_BUTTON_SIZE - CLOSE_BUTTON_MARGIN;
            int close_y = (TAB_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
            XSetForeground(term->display, term->gc, 0x000000);
            XDrawLine(term->display, term->window, term->gc, close_x, close_y, close_x + CLOSE_BUTTON_SIZE, close_y + CLOSE_BUTTON_SIZE);
            XDrawLine(term->display, term->window, term->gc, close_x + CLOSE_BUTTON_SIZE, close_y, close_x, close_y + CLOSE_BUTTON_SIZE);
        }
        display_idx++;
    }

    int plus_x = (display_idx < visible_tabs) ? display_idx * TAB_WIDTH : (visible_tabs * TAB_WIDTH);
    if (plus_x + PLUS_TAB_WIDTH <= term->window_width) {
        XSetForeground(term->display, term->gc, 0x555555);
        XFillRectangle(term->display, term->window, term->gc, plus_x, 0, PLUS_TAB_WIDTH, TAB_HEIGHT);
        XSetForeground(term->display, term->gc, 0x000000);
        XDrawString(term->display, term->window, term->gc, plus_x + 12, 20, "+", 1);
    }

    XSetForeground(term->display, term->gc, 0xFFFFFF);
    XDrawLine(term->display, term->window, term->gc, 0, TAB_HEIGHT, term->window_width, TAB_HEIGHT);

    Tab *tab = term->tabs[term->current_tab];
    int y = TAB_HEIGHT + 20;
    XSetForeground(term->display, term->gc, term->grey_color);

    char temp_buffer[MAX_BUFFER];
    if (tab->buffer_len >= MAX_BUFFER) tab->buffer_len = MAX_BUFFER - 1;
    memcpy(temp_buffer, tab->buffer, tab->buffer_len);
    temp_buffer[tab->buffer_len] = '\0';

    int total_lines = 0;
    char *ptr = temp_buffer;
    if (*ptr) {
        char *s = ptr;
        while (1) {
            char *nl = strchr(s, '\n');
            if (nl) { total_lines++; s = nl + 1; }
            else { if (*s) total_lines++; break; }
        }
    }

    int available_height = term->window_height - TAB_HEIGHT - 60;
    int max_visible_lines = available_height / FONT_HEIGHT;
    if (max_visible_lines <= 0) max_visible_lines = 1;

    if (!tab->user_scrolling) {
        if (total_lines > max_visible_lines) tab->scroll_offset = total_lines - max_visible_lines;
        else tab->scroll_offset = 0;
    } else {
        if (tab->scroll_offset < 0) tab->scroll_offset = 0;
        int max_offset = (total_lines > max_visible_lines) ? (total_lines - max_visible_lines) : 0;
        if (tab->scroll_offset > max_offset) tab->scroll_offset = max_offset;
    }

    ptr = temp_buffer;
    int line_count = 0;
    char *line = strtok(ptr, "\n");
    while (line != NULL && y < term->window_height - 60) {
        if (line_count >= tab->scroll_offset) {
            XDrawString(term->display, term->window, term->gc, 10, y, line, (int)strlen(line));
            y += FONT_HEIGHT;
        }
        line_count++;
        line = strtok(NULL, "\n");
    }

    char display_line[4096];
    const char *prompt = tab->search_mode ? "Enter search term: " : (tab->continuation_mode ? CONTINUE_PROMPT : PROMPT);

    if (tab->search_mode) {
        snprintf(display_line, sizeof(display_line), "%s%s", prompt, tab->search_input);
    } else {
        snprintf(display_line, sizeof(display_line), "%s%.4000s", prompt, tab->input);
    }
    display_line[sizeof(display_line)-1] = '\0';
    XDrawString(term->display, term->window, term->gc, 10, y, display_line, (int)strlen(display_line));

    int cursor_display_pos = tab->search_mode ? (int)strlen(prompt) + tab->search_input_len : (int)strlen(prompt) + tab->cursor_pos;
    int cursor_x = 10 + XTextWidth(term->font, display_line, (cursor_display_pos < (int)strlen(display_line) ? cursor_display_pos : (int)strlen(display_line)));
    if (cursor_x < 10) cursor_x = 10;
    if (cursor_x > term->window_width - 20) cursor_x = term->window_width - 20;
    XDrawLine(term->display, term->window, term->gc, cursor_x, y - FONT_HEIGHT + 3, cursor_x, y + 2);

    XFlush(term->display);
}

void handle_mouse_click(Terminal *term, XButtonEvent *event) {
    if (event->y <= TAB_HEIGHT) {
        int visible_tabs = (term->window_width - PLUS_TAB_WIDTH) / TAB_WIDTH;
        if (visible_tabs <= 0) visible_tabs = 1;
        if (event->x < visible_tabs * TAB_WIDTH && event->x < term->num_tabs * TAB_WIDTH) {
            int clicked_idx = event->x / TAB_WIDTH;
            int start_tab = 0;
            if (term->num_tabs > visible_tabs) {
                start_tab = term->current_tab - visible_tabs / 2;
                if (start_tab < 0) start_tab = 0;
                if (start_tab + visible_tabs > term->num_tabs) start_tab = term->num_tabs - visible_tabs;
            }
            int actual_tab = start_tab + clicked_idx;
            if (actual_tab < term->num_tabs) {
                int tab_x = clicked_idx * TAB_WIDTH;
                int close_x = tab_x + TAB_WIDTH - CLOSE_BUTTON_SIZE - CLOSE_BUTTON_MARGIN;
                int close_y = (TAB_HEIGHT - CLOSE_BUTTON_SIZE) / 2;
                if (term->num_tabs > 1 &&
                    event->x >= close_x - 2 && event->x <= close_x + CLOSE_BUTTON_SIZE + 2 &&
                    event->y >= close_y - 2 && event->y <= close_y + CLOSE_BUTTON_SIZE + 2) {
                    close_tab(term, actual_tab);
                } else {
                    term->current_tab = actual_tab;
                }
                draw_terminal(term);
            }
        } else {
            add_new_tab(term);
            draw_terminal(term);
        }
    }
}

void handle_mouse_scroll(Terminal *term, XButtonEvent *event) {
    Tab *tab = term->tabs[term->current_tab];
    if (event->button == Button4) { tab->user_scrolling = 1; tab->scroll_offset += 1; }
    else if (event->button == Button5) {
        if (tab->scroll_offset > 0) tab->scroll_offset -= 1;
        else { tab->scroll_offset = 0; tab->user_scrolling = 0; }
    }
    draw_terminal(term);
}

void handle_keypress(Terminal *term, XKeyEvent *event) {
    Tab *tab = term->tabs[term->current_tab];
    char buffer[64];
    KeySym keysym;
    Status status;
    int len;
    if (term->xic != NULL) len = Xutf8LookupString(term->xic, event, buffer, sizeof(buffer)-1, &keysym, &status);
    else len = XLookupString(event, buffer, sizeof(buffer)-1, &keysym, NULL);
    buffer[len] = '\0';

    if ((event->state & ControlMask) && (event->state & ShiftMask) && keysym == XK_C) {
        XSetSelectionOwner(term->display, CLIPBOARD, term->window, CurrentTime);
        XChangeProperty(term->display, term->window, CLIPBOARD,
                        UTF8_STRING, 8, PropModeReplace,
                        (unsigned char *)tab->input, strlen(tab->input));

        add_to_buffer(tab, "[Copied to system clipboard]\n");
        draw_terminal(term);
        return;
    }
    if ((event->state & ControlMask) && (event->state & ShiftMask) && keysym == XK_V) {
        XConvertSelection(term->display, CLIPBOARD, UTF8_STRING, CLIPBOARD, term->window, CurrentTime);
        XFlush(term->display);
        return;
    }

    if (keysym == XK_q && (event->state & ControlMask)) {
        history_save_to_file();
        for (int i = 0; i < history_count; ++i) free(command_history[i]);
        cleanup_temp_files();
        exit(0);
    }

    if (keysym == XK_c && (event->state & ControlMask)) {
        if (current_fg_pid > 0) {
            kill(-current_fg_pid, SIGINT);
            add_to_buffer(term->tabs[term->current_tab], "^C\n");
            draw_terminal(term);
        }
        return;
    }

    if (tab->search_mode) {
        if (keysym == XK_Return) {
            tab->search_input[tab->search_input_len] = '\0';
            add_to_buffer(tab, "\n");
            perform_history_search(tab);
            tab->search_mode = 0;
            tab->search_input_len = 0;
            tab->search_input[0] = '\0';
            add_to_buffer(tab, PROMPT);
            draw_terminal(term);
            return;
        } else if (keysym == XK_BackSpace) {
            if (tab->search_input_len > 0) {
                tab->search_input_len--;
                tab->search_input[tab->search_input_len] = '\0';
            }
            draw_terminal(term);
            return;
        } else if (len > 0) {
            if (tab->search_input_len + len < (int)sizeof(tab->search_input) - 1) {
                memcpy(tab->search_input + tab->search_input_len, buffer, len);
                tab->search_input_len += len;
                tab->search_input[tab->search_input_len] = '\0';
            }
            draw_terminal(term);
            return;
        } else return;
    }

    if (tab->ac_waiting_choice) {
        if (len > 0) {
            int idx = atoi(buffer);
            if (idx >= 1 && idx <= tab->ac_count) {
                const char *chosen = tab->ac_matches[idx - 1];
                int start = token_start_index(tab);
                int pos = tab->cursor_pos;
                if (start < 0) start = 0;
                if (pos < start) pos = start;
                memmove(tab->input + start, tab->input + pos, tab->input_len - pos + 1);
                tab->input_len -= (pos - start);
                tab->cursor_pos = start;
                int name_len = (int)strlen(chosen);
                if (tab->input_len + name_len < (int)sizeof(tab->input) - 1) {
                    memmove(tab->input + start + name_len, tab->input + start, tab->input_len - start + 1);
                    memcpy(tab->input + start, chosen, name_len);
                    tab->input_len += name_len;
                    tab->cursor_pos = start + name_len;
                }
                add_to_buffer(term->tabs[term->current_tab], "\n");
                add_to_buffer(term->tabs[term->current_tab], PROMPT);
                free_autocomplete(tab);
                draw_terminal(term);
                return;
            } else {
                add_to_buffer(tab, "\nInvalid choice\n");
                add_to_buffer(tab, PROMPT);
                free_autocomplete(tab);
                draw_terminal(term);
                return;
            }
        }
        return;
    }

    if (keysym == XK_r && (event->state & ControlMask)) {
        tab->search_mode = 1;
        tab->search_input_len = 0;
        tab->search_input[0] = '\0';
        draw_terminal(term);
        return;
    }

    if (keysym == XK_t && (event->state & ControlMask)) { add_new_tab(term); draw_terminal(term); return; }

    if (keysym == XK_a && (event->state & ControlMask)) { tab->cursor_pos = 0; draw_terminal(term); return; }
    if (keysym == XK_e && (event->state & ControlMask)) { tab->cursor_pos = tab->input_len; draw_terminal(term); return; }

    if ((event->state & ControlMask) && (keysym == XK_Up || keysym == XK_Down)) {
        int total_lines = 0;
        for (int i = 0; i < term->tabs[term->current_tab]->buffer_len; ++i) if (term->tabs[term->current_tab]->buffer[i] == '\n') total_lines++;
        if (term->tabs[term->current_tab]->buffer_len > 0 && term->tabs[term->current_tab]->buffer[term->tabs[term->current_tab]->buffer_len - 1] != '\n') total_lines++;
        int available_height = term->window_height - TAB_HEIGHT - 60;
        int max_visible_lines = (available_height / FONT_HEIGHT) > 0 ? (available_height / FONT_HEIGHT) : 1;
        int max_offset = total_lines > max_visible_lines ? total_lines - max_visible_lines : 0;
        if (keysym == XK_Up) { tab->user_scrolling = 1; tab->scroll_offset += 1; if (tab->scroll_offset > max_offset) tab->scroll_offset = max_offset; }
        else { tab->scroll_offset -= 1; if (tab->scroll_offset <= 0) { tab->scroll_offset = 0; tab->user_scrolling = 0; } }
        draw_terminal(term);
        return;
    }

    if (event->state & Mod1Mask) {
        if (keysym >= XK_1 && keysym <= XK_9) {
            int tab_num = keysym - XK_1;
            if (tab_num < term->num_tabs) { term->current_tab = tab_num; draw_terminal(term); return; }
        }
    }

    if (keysym == XK_Tab) {
        handle_autocomplete(tab, term);
        return;
    }

    if (keysym == XK_Return) {
        if (tab->input_len > 0) {
            int ends_with_backslash = 0;
            if (tab->input_len >= 1 && tab->input[tab->input_len - 1] == '\\') ends_with_backslash = 1;
            if (ends_with_backslash) {
                tab->input[tab->input_len - 1] = '\0';
                tab->input_len--;
                if (tab->full_command_len + tab->input_len < (int)sizeof(tab->full_command) - 2) {
                    int avail = sizeof(tab->full_command) - tab->full_command_len - 1;
                    if (avail > 0) {
                        strncat(tab->full_command, tab->input, avail);
                        tab->full_command_len = strlen(tab->full_command);
                    }
                    tab->full_command_len += tab->input_len;
                }
                const char *prompt = tab->continuation_mode ? CONTINUE_PROMPT : PROMPT;
                add_to_buffer(tab, prompt);
                add_to_buffer(tab, tab->input);
                add_to_buffer(tab, "\n");
                tab->continuation_mode = 1;
                memset(tab->input, 0, sizeof(tab->input));
                tab->input_len = 0; tab->cursor_pos = 0;
            } else {
                if (tab->continuation_mode) {
                    if (tab->full_command_len + tab->input_len < (int)sizeof(tab->full_command) - 1) {
                        int avail = sizeof(tab->full_command) - tab->full_command_len - 1;
                        if (avail > 0) {
                            strncat(tab->full_command, tab->input, avail);
                            tab->full_command_len = strlen(tab->full_command);
                        }
                        tab->full_command_len += tab->input_len;
                    }
                    add_to_buffer(tab, CONTINUE_PROMPT);
                    add_to_buffer(tab, tab->input);
                    add_to_buffer(tab, "\n");
                    process_command(term, tab->full_command);
                    tab->continuation_mode = 0;
                    memset(tab->full_command, 0, sizeof(tab->full_command));
                    tab->full_command_len = 0;
                } else {
                    add_to_buffer(tab, tab->input);
                    add_to_buffer(tab, "\n");
                    process_command(term, tab->input);
                }
                memset(tab->input, 0, sizeof(tab->input));
                tab->input_len = 0; tab->cursor_pos = 0;
            }
        } else {
            add_to_buffer(tab, "\n");
            add_to_buffer(tab, PROMPT);
        }
        draw_terminal(term);
        return;
    }

    if (keysym == XK_BackSpace) {
        if (tab->cursor_pos > 0) {
            int bytes_to_delete = 1;
            unsigned char c = (unsigned char)tab->input[tab->cursor_pos - 1];
            if ((c & 0x80) != 0) {
                while (tab->cursor_pos - bytes_to_delete > 0 && (tab->input[tab->cursor_pos - bytes_to_delete] & 0xC0) == 0x80) bytes_to_delete++;
            }
            memmove(&tab->input[tab->cursor_pos - bytes_to_delete], &tab->input[tab->cursor_pos], tab->input_len - tab->cursor_pos + 1);
            tab->cursor_pos -= bytes_to_delete;
            tab->input_len -= bytes_to_delete;
        }
        draw_terminal(term);
        return;
    }

    if (keysym == XK_Left) {
        if (tab->cursor_pos > 0) {
            tab->cursor_pos--;
            while (tab->cursor_pos > 0 && (tab->input[tab->cursor_pos] & 0xC0) == 0x80) tab->cursor_pos--;
        }
        draw_terminal(term); return;
    }
    if (keysym == XK_Right) {
        if (tab->cursor_pos < tab->input_len) {
            tab->cursor_pos++;
            while (tab->cursor_pos < tab->input_len && (tab->input[tab->cursor_pos] & 0xC0) == 0x80) tab->cursor_pos++;
        }
        draw_terminal(term); return;
    }

    if (!(event->state & ControlMask) && keysym == XK_Up) {
        if (history_count == 0) return;
        if (history_nav_index == history_count) history_nav_index = history_count - 1;
        else if (history_nav_index > 0) history_nav_index--;
        const char *h = command_history[history_nav_index];
        if (h) {
            strncpy(tab->input, h, sizeof(tab->input)-1);
            tab->input[sizeof(tab->input)-1] = '\0';
            tab->input_len = (int)strlen(tab->input);
            tab->cursor_pos = tab->input_len;
            draw_terminal(term);
        }
        return;
    }
    if (!(event->state & ControlMask) && keysym == XK_Down) {
        if (history_count == 0) return;
        if (history_nav_index < history_count - 1) {
            history_nav_index++;
            const char *h = command_history[history_nav_index];
            strncpy(tab->input, h ? h : "", sizeof(tab->input)-1);
            tab->input[sizeof(tab->input)-1] = '\0';
            tab->input_len = (int)strlen(tab->input);
            tab->cursor_pos = tab->input_len;
        } else {
            history_nav_index = history_count;
            memset(tab->input, 0, sizeof(tab->input));
            tab->input_len = 0; tab->cursor_pos = 0;
        }
        draw_terminal(term);
        return;
    }

    if (len > 0 && (tab->input_len + len < (int)sizeof(tab->input) - 1)) {
        memmove(&tab->input[tab->cursor_pos + len], &tab->input[tab->cursor_pos], tab->input_len - tab->cursor_pos + 1);
        memcpy(&tab->input[tab->cursor_pos], buffer, len);
        tab->cursor_pos += len;
        tab->input_len += len;
        tab->input[tab->input_len] = '\0';
        draw_terminal(term);
    }
}
#endif
