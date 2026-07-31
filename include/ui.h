#ifndef UI_H
#define UI_H

#include "common.h"

#ifndef WITHOUT_X11
extern Atom CLIPBOARD;
extern Atom UTF8_STRING;

void draw_terminal(Terminal *term);
void handle_mouse_click(Terminal *term, XButtonEvent *event);
void handle_mouse_scroll(Terminal *term, XButtonEvent *event);
void handle_keypress(Terminal *term, XKeyEvent *event);
#endif

#endif /* UI_H */
