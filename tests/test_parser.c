#ifndef WITHOUT_X11
#define WITHOUT_X11
#endif
#include "test_harness.h"
#include "exec.h"
#include "common.h"

void test_parse_redirection(void) {
    printf("\n--- Running Redirection Parsing Tests ---\n");
    char cmd1[] = "ls -la > output.txt";
    char *input_file = NULL;
    char *output_file = NULL;

    parse_redirection(cmd1, &input_file, &output_file);
    TEST_ASSERT_EQUALS_STR("ls -la ", cmd1, "Command truncated at output redirection symbol");
    TEST_ASSERT_EQUALS_STR("output.txt", output_file, "Output file parsed correctly");
    TEST_ASSERT(input_file == NULL, "Input file remains NULL when not redirected");

    char cmd2[] = "cat < input.dat > processed.log";
    input_file = NULL;
    output_file = NULL;
    parse_redirection(cmd2, &input_file, &output_file);
    TEST_ASSERT_EQUALS_STR("processed.log", output_file, "Output redirection parsed in combined command");
    TEST_ASSERT_EQUALS_STR("input.dat", input_file, "Input redirection parsed in combined command");
}

void test_parse_arguments(void) {
    printf("\n--- Running Argument Tokenization Tests ---\n");
    char cmd[] = "gcc -O2 \"my folder/file.c\" -o myterm";
    char *args[16];
    int argc = parse_arguments(cmd, args, 16);

    TEST_ASSERT_EQUALS_INT(5, argc, "Correct number of arguments parsed (handling quotes)");
    TEST_ASSERT_EQUALS_STR("gcc", args[0], "Arg 0 correct");
    TEST_ASSERT_EQUALS_STR("-O2", args[1], "Arg 1 correct");
    TEST_ASSERT_EQUALS_STR("my folder/file.c", args[2], "Arg 2 keeps spaces inside quotes");
    TEST_ASSERT_EQUALS_STR("-o", args[3], "Arg 3 correct");
    TEST_ASSERT_EQUALS_STR("myterm", args[4], "Arg 4 correct");
    TEST_ASSERT(args[5] == NULL, "Null termination at argc");

    char echo_cmd[] = "echo Hello World";
    char *echo_args[10];
    int echo_argc = parse_arguments(echo_cmd, echo_args, 10);
    TEST_ASSERT_EQUALS_INT(4, echo_argc, "echo automatically injects -e argument");
    TEST_ASSERT_EQUALS_STR("echo", echo_args[0], "echo Arg 0 correct");
    TEST_ASSERT_EQUALS_STR("-e", echo_args[1], "echo Arg 1 is -e");
    TEST_ASSERT_EQUALS_STR("Hello", echo_args[2], "echo Arg 2 correct");
    TEST_ASSERT_EQUALS_STR("World", echo_args[3], "echo Arg 3 correct");
}

void test_longest_common_prefix(void) {
    printf("\n--- Running Autocomplete Longest Common Prefix Tests ---\n");
    char *matches1[] = {"Makefile", "Make.h", "Make_run.sh"};
    char out[128];
    longest_common_prefix(matches1, 3, out, sizeof(out));
    TEST_ASSERT_EQUALS_STR("Make", out, "LCP across multiple files matching 'Make'");

    char *matches2[] = {"test_lcs.c", "test_parser.c"};
    longest_common_prefix(matches2, 2, out, sizeof(out));
    TEST_ASSERT_EQUALS_STR("test_", out, "LCP matching 'test_' prefix");

    char *matches3[] = {"alpha", "beta"};
    longest_common_prefix(matches3, 2, out, sizeof(out));
    TEST_ASSERT_EQUALS_STR("", out, "Disjoint string set returns empty prefix");
}

int main(void) {
    printf("==========================================\n");
    printf("    MyTerm Parser - Unit Test Suite       \n");
    printf("==========================================\n");

    test_parse_redirection();
    test_parse_arguments();
    test_longest_common_prefix();

    TEST_SUMMARY("Command Parser & Autocomplete");
    return 0;
}
