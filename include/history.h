#ifndef HISTORY_H
#define HISTORY_H

#include "common.h"

extern char *command_history[MAX_HISTORY];
extern int history_count;
extern int history_nav_index;

void history_load_from_file(void);
void history_save_to_file(void);
void add_history(const char *cmd);

/* Exposed for unit testing and search */
int longest_common_substring_len(const char *a, const char *b);
const char *search_history_lcs(const char *query, int *is_exact);
void perform_history_search(Tab *tab);

#endif /* HISTORY_H */
