#include <locale.h>
#include "common.h"
#include "io.h"
#include "history.h"
#include "exec.h"
#include "ui.h"

#ifndef WITHOUT_X11
Atom CLIPBOARD;
Atom UTF8_STRING;

int main(void) {
    setlocale(LC_ALL, "");

    history_load_from_file();

    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);

    Terminal term;
    term.display = XOpenDisplay(NULL);
    if (term.display == NULL) {
        fprintf(stderr, "Cannot open X display\n");
        history_save_to_file();
        return 1;
    }
    CLIPBOARD = XInternAtom(term.display, "CLIPBOARD", False);
    UTF8_STRING = XInternAtom(term.display, "UTF8_STRING", False);

    term.screen = DefaultScreen(term.display);

    term.window_width = INITIAL_WINDOW_WIDTH;
    term.window_height = INITIAL_WINDOW_HEIGHT;

    term.xim = XOpenIM(term.display, NULL, NULL, NULL);
    if (term.xim != NULL) {
        term.xic = XCreateIC(term.xim,
                            XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                            XNClientWindow, term.window,
                            XNFocusWindow, term.window,
                            NULL);
        if (term.xic != NULL)
            XSetICFocus(term.xic);
    } else {
        term.xic = NULL;
    }

    term.window = XCreateSimpleWindow(term.display, RootWindow(term.display, term.screen),
                                      100, 100, term.window_width, term.window_height, 1,
                                      BlackPixel(term.display, term.screen), 0x000000);
    Atom wmDeleteMessage = XInternAtom(term.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(term.display, term.window, &wmDeleteMessage, 1);

    XStoreName(term.display, term.window, "MyTerm - Custom Terminal");
    XSelectInput(term.display, term.window, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    if (term.xim != NULL) {
        term.xic = XCreateIC(term.xim,
                             XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                             XNClientWindow, term.window,
                             XNFocusWindow, term.window,
                             NULL);
        if (term.xic != NULL) XSetICFocus(term.xic);
    }

    term.gc = XCreateGC(term.display, term.window, 0, NULL);

    Colormap colormap = DefaultColormap(term.display, term.screen);
    XColor color;
    XParseColor(term.display, colormap, "#AAAAAA", &color);
    XAllocColor(term.display, colormap, &color);
    term.grey_color = color.pixel;

    term.font = XLoadQueryFont(term.display, FONT_NAME);
    if (term.font == NULL) term.font = XLoadQueryFont(term.display, "*");
    if (term.font != NULL) XSetFont(term.display, term.gc, term.font->fid);

    term.tab_capacity = 10;
    term.tabs = malloc(term.tab_capacity * sizeof(Tab*));
    term.num_tabs = 1;
    term.current_tab = 0;
    term.tabs[0] = malloc(sizeof(Tab));
    init_tab(term.tabs[0]);

    XMapWindow(term.display, term.window);
    XFlush(term.display);

    XEvent event;
    int running = 1;

    printf("╔═══════════════════════════════════════════════════╗\n");
    printf("║    MyTerm - ALL FEATURES WORKING + FEATURE 11     ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n\n");
    printf("Use Ctrl+Q to quit (saves history). Use Ctrl+R for history search (exact then LCS fallback).\n");
    printf("Tab -> filename autocomplete (Feature 11). When listing choices, press the number (1..9) to pick.\n\n");

    while (running) {
        XNextEvent(term.display, &event);
        switch (event.type) {
            case Expose:
                if (event.xexpose.count == 0) draw_terminal(&term);
                break;
            case ConfigureNotify:
                if (event.xconfigure.width != term.window_width || event.xconfigure.height != term.window_height) {
                    term.window_width = event.xconfigure.width;
                    term.window_height = event.xconfigure.height;
                    draw_terminal(&term);
                }
                break;
            case KeyPress:
                handle_keypress(&term, &event.xkey);
                break;
            case ButtonPress:
                if (event.xbutton.button == Button4 || event.xbutton.button == Button5) handle_mouse_scroll(&term, &event.xbutton);
                else handle_mouse_click(&term, &event.xbutton);
                break;
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == wmDeleteMessage) {
                    history_save_to_file();
                    cleanup_temp_files();
                    for (int i = 0; i < history_count; ++i) free(command_history[i]);
                    for (int i = 0; i < term.num_tabs; ++i) free_autocomplete(term.tabs[i]);
                    XCloseDisplay(term.display);
                    exit(0);
                }
                break;
            case DestroyNotify:
                history_save_to_file();
                cleanup_temp_files();
                for (int i = 0; i < history_count; ++i) free(command_history[i]);
                for (int i = 0; i < term.num_tabs; ++i) free_autocomplete(term.tabs[i]);
                XCloseDisplay(term.display);
                exit(0);
            case SelectionNotify: {
                if (event.xselection.property == None) break;

                Atom type;
                int format;
                unsigned long nitems, bytes_after;
                unsigned char *data = NULL;

                XGetWindowProperty(term.display, term.window, CLIPBOARD, 0, (~0L),
                                False, UTF8_STRING, &type, &format, &nitems,
                                &bytes_after, &data);

                if (data) {
                    Tab *tab = term.tabs[term.current_tab];
                    size_t len = strlen((char *)data);
                    if (tab->input_len + len < sizeof(tab->input) - 1) {
                        strcat(tab->input, (char *)data);
                        tab->input_len += len;
                        draw_terminal(&term);
                    }
                    XFree(data);
                }
                break;
            }
            default:
                break;
        }
    }

    if (term.xic) XDestroyIC(term.xic);
    if (term.xim) XCloseIM(term.xim);

    for (int i = 0; i < term.num_tabs; i++) free(term.tabs[i]);
    free(term.tabs);
    if (term.font != NULL) XFreeFont(term.display, term.font);
    XFreeGC(term.display, term.gc);
    XDestroyWindow(term.display, term.window);
    XCloseDisplay(term.display);

    history_save_to_file();
    for (int i = 0; i < history_count; ++i) free(command_history[i]);
    cleanup_temp_files();

    return 0;
}
#endif
