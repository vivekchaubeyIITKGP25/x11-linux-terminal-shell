#ifndef WITHOUT_X11
#define WITHOUT_X11
#endif
#include "test_harness.h"
#include "history.h"
#include "common.h"

void test_lcs_length(void) {
    printf("\n--- Running LCS Length Tests ---\n");
    TEST_ASSERT_EQUALS_INT(4, longest_common_substring_len("test", "test"), "Identical strings match full length");
    TEST_ASSERT_EQUALS_INT(4, longest_common_substring_len("myterm", "terminal"), "Partial overlap ('term') has length 4");
    TEST_ASSERT_EQUALS_INT(6, longest_common_substring_len("gcc myterm.c -Wall", "myterm"), "Substring match ('myterm') has length 6");
    TEST_ASSERT_EQUALS_INT(0, longest_common_substring_len("abc", "xyz"), "Disjoint strings return 0");
    TEST_ASSERT_EQUALS_INT(0, longest_common_substring_len("", "test"), "Empty string returns 0");
}

void test_history_search(void) {
    printf("\n--- Running History Search & LCS Fallback Tests ---\n");
    history_count = 0;
    add_history("ls -la /usr/bin");
    add_history("gcc -Wall myterm.c -o myterm");
    add_history("git status");
    add_history("cat ~/.myterm_history");

    int is_exact = 0;
    const char *res;

    res = search_history_lcs("git status", &is_exact);
    TEST_ASSERT(res != NULL, "Exact search should find result");
    TEST_ASSERT_EQUALS_INT(1, is_exact, "Exact search sets is_exact flag to 1");
    TEST_ASSERT_EQUALS_STR("git status", res, "Exact match returns correct command");

    /* Partial match using LCS fallback - prioritizes most recent matching command */
    res = search_history_lcs("myterm", &is_exact);
    TEST_ASSERT(res != NULL, "LCS search should find result for partial query");
    TEST_ASSERT_EQUALS_INT(0, is_exact, "LCS fallback sets is_exact flag to 0");
    TEST_ASSERT_EQUALS_STR("cat ~/.myterm_history", res, "LCS fallback correctly prefers the most recent matching command in history");

    res = search_history_lcs("gcc", &is_exact);
    TEST_ASSERT_EQUALS_STR("gcc -Wall myterm.c -o myterm", res, "LCS fallback finds specific earlier command when query uniquely matches");

    /* No match (LCS <= 2) */
    res = search_history_lcs("xy", &is_exact);
    TEST_ASSERT(res == NULL, "Short/non-matching search term (LCS <= 2) returns NULL");

    for (int i = 0; i < history_count; ++i) {
        free(command_history[i]);
    }
    history_count = 0;
}

int main(void) {
    printf("==========================================\n");
    printf("       MyTerm LCS - Unit Test Suite       \n");
    printf("==========================================\n");

    test_lcs_length();
    test_history_search();

    TEST_SUMMARY("LCS & History Search");
    return 0;
}
