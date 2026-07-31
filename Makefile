CC ?= gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lX11 -lXext

SRCS = src/history.c src/io.c src/exec.c src/ui.c src/main.c
OBJS = $(SRCS:.c=.o)
TARGET = myterm

TEST_FLAGS = -Wall -Wextra -g -I./include -I./tests -DWITHOUT_X11
TEST_LCS = tests/test_lcs
TEST_PARSER = tests/test_parser

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Unit test compilation (completely decoupled from X11 GUI dependencies)
$(TEST_LCS): tests/test_lcs.c src/history.c src/io.c
	$(CC) $(TEST_FLAGS) $^ -o $@

$(TEST_PARSER): tests/test_parser.c src/exec.c src/io.c src/history.c
	$(CC) $(TEST_FLAGS) $^ -o $@

test: check
check: $(TEST_LCS) $(TEST_PARSER)
	@echo "=========================================="
	@echo "       Running Automated Test Suite       "
	@echo "=========================================="
	@./$(TEST_LCS)
	@./$(TEST_PARSER)
	@echo "All tests completed successfully!"

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_LCS) $(TEST_PARSER) tests/*.o .temp.*

.PHONY: all test check clean
