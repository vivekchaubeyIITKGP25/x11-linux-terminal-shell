#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  [FAIL] %s (File: %s, Line: %d)\n", msg, __FILE__, __LINE__); \
    } \
} while (0)

#define TEST_ASSERT_EQUALS_INT(expected, actual, msg) do { \
    tests_run++; \
    int _exp = (expected); \
    int _act = (actual); \
    if (_exp == _act) { \
        tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  [FAIL] %s (Expected: %d, Actual: %d at %s:%d)\n", msg, _exp, _act, __FILE__, __LINE__); \
    } \
} while (0)

#define TEST_ASSERT_EQUALS_STR(expected, actual, msg) do { \
    tests_run++; \
    const char* _exp = (expected); \
    const char* _act = (actual); \
    if (_exp && _act && strcmp(_exp, _act) == 0) { \
        tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  [FAIL] %s (Expected: '%s', Actual: '%s' at %s:%d)\n", msg, _exp ? _exp : "NULL", _act ? _act : "NULL", __FILE__, __LINE__); \
    } \
} while (0)

#define TEST_SUMMARY(suite_name) do { \
    printf("\n=== %s Test Summary ===\n", suite_name); \
    printf("Total: %d | Passed: %d | Failed: %d\n\n", tests_run, tests_passed, tests_failed); \
    if (tests_failed > 0) exit(1); \
} while (0)

#endif /* TEST_HARNESS_H */
