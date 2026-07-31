#ifndef EXEC_H
#define EXEC_H

#include "common.h"

void process_command(Terminal *term, const char *command);
void execute_external_command(Terminal *term, const char *command);
void execute_piped_command(Terminal *term, const char *command);
void execute_multiwatch(Terminal *term, const char *command);
void cleanup_temp_files(void);

/* Helper functions exposed for unit testing and modular clarity */
void longest_common_prefix(char **arr, int n, char *out, size_t out_size);
int token_start_index(const Tab *tab);
int parse_redirection(char *cmd_copy, char **input_file, char **output_file);
int parse_arguments(char *cmd_copy, char *args[], int max_args);

#ifndef WITHOUT_X11
void handle_autocomplete(Tab *tab, Terminal *term);
#endif

#endif /* EXEC_H */
