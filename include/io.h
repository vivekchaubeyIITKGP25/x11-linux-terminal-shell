#ifndef IO_H
#define IO_H

#include "common.h"

void sigint_handler(int sig);
void sigtstp_handler(int sig);
void multiwatch_signal_handler(int sig);

void init_tab(Tab *tab);
void add_new_tab(Terminal *term);
void close_tab(Terminal *term, int tab_index);
void trim_buffer(Tab *tab);
void add_to_buffer(Tab *tab, const char *text);
void free_autocomplete(Tab *tab);

#endif /* IO_H */
