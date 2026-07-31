#include "history.h"
#include "io.h"

char *command_history[MAX_HISTORY];
int history_count = 0;
int history_nav_index = 0;

void history_load_from_file(void) {
    const char *home = getenv("HOME");
    if (!home) return;

    char history_path[512];
    snprintf(history_path, sizeof(history_path), "%s/.myterm_history", home);

    FILE *f = fopen(history_path, "r");
    if (!f) return;

    char line[4096];
    while (fgets(line, sizeof(line), f) && history_count < MAX_HISTORY) {
        line[strcspn(line, "\n")] = 0;  /* remove newline */
        if (strlen(line) > 0) {
            command_history[history_count++] = strdup(line);
        }
    }

    fclose(f);
}

void history_save_to_file(void) {
    const char *home = getenv("HOME");
    if (!home) return;

    char history_path[512];
    snprintf(history_path, sizeof(history_path), "%s/.myterm_history", home);

    FILE *f = fopen(history_path, "w");
    if (!f) return;

    for (int i = 0; i < history_count; ++i) {
        if (command_history[i] && strlen(command_history[i]) > 0) {
            fprintf(f, "%s\n", command_history[i]);
        }
    }

    fflush(f);
    fclose(f);
}

void add_history(const char *cmd) {
    if (!cmd || *cmd == '\0') return;
    if (history_count < MAX_HISTORY) {
        command_history[history_count++] = strdup(cmd);
    } else {
        free(command_history[0]);
        memmove(command_history, command_history + 1, (MAX_HISTORY - 1) * sizeof(char *));
        command_history[MAX_HISTORY - 1] = strdup(cmd);
    }
    history_nav_index = history_count;
}

int longest_common_substring_len(const char *a, const char *b) {
    if (!a || !b) return 0;
    int na = (int)strlen(a);
    int nb = (int)strlen(b);
    if (na == 0 || nb == 0) return 0;

    int *prev = (int*)calloc(nb + 1, sizeof(int));
    int *cur  = (int*)calloc(nb + 1, sizeof(int));
    if (!prev || !cur) { free(prev); free(cur); return 0; }

    int best = 0;
    for (int i = 1; i <= na; ++i) {
        for (int j = 1; j <= nb; ++j) {
            if (a[i-1] == b[j-1]) {
                cur[j] = prev[j-1] + 1;
                if (cur[j] > best) best = cur[j];
            } else cur[j] = 0;
        }
        int *tmp = prev; prev = cur; cur = tmp;
        memset(cur, 0, (nb + 1) * sizeof(int));
    }

    free(prev);
    free(cur);
    return best;
}

const char *search_history_lcs(const char *query, int *is_exact) {
    if (!query || strlen(query) == 0) return NULL;
    if (is_exact) *is_exact = 0;

    /* exact match first */
    for (int i = history_count - 1; i >= 0; --i) {
        if (command_history[i] && strcmp(command_history[i], query) == 0) {
            if (is_exact) *is_exact = 1;
            return command_history[i];
        }
    }

    /* LCS fallback */
    int max_lcs = 0;
    int best_index = -1;
    for (int i = history_count - 1; i >= 0; --i) {
        if (!command_history[i]) continue;
        int lcs = longest_common_substring_len(query, command_history[i]);
        if (lcs > max_lcs) {
            max_lcs = lcs;
            best_index = i;
        }
    }

    if (max_lcs > 2 && best_index >= 0) {
        return command_history[best_index];
    }
    return NULL;
}

void perform_history_search(Tab *tab) {
    const char *q = tab->search_input;
    if (!q || strlen(q) == 0) { 
        add_to_buffer(tab, "Empty search term\n"); 
        return; 
    }

    int is_exact = 0;
    const char *result = search_history_lcs(q, &is_exact);
    if (result) {
        char out[4096];
        snprintf(out, sizeof(out)-1, "%s\n", result);
        out[sizeof(out)-1] = '\0';
        add_to_buffer(tab, out);
    } else {
        add_to_buffer(tab, "No match for search term in history\n");
    }
}
