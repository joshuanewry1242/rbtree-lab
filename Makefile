CC       := gcc
CFLAGS   := -std=c23 -Wall -Wextra -Werror -g -O1 -Iinclude -Itests
SRC      := src/rbtree.c src/pool.c
TSRC     := tests/test_rbtree.c tests/fault_alloc.c
BIN      := build/test_rbtree
FUZZBIN  := build/fuzz
# node count for the big M3 tests under valgrind (full size is 10^6)
BIG_N    ?= 20000

all: $(BIN) $(FUZZBIN)

$(BIN): $(SRC) $(TSRC) include/rbtree.h src/pool.h tests/fault_alloc.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) $(TSRC) -o $@ -lpthread

$(FUZZBIN): $(SRC) tests/fuzz.c tests/fault_alloc.c include/rbtree.h src/pool.h tests/fault_alloc.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) tests/fuzz.c tests/fault_alloc.c -o $@ -lpthread

test: $(BIN) $(FUZZBIN)
	./$(BIN) && ./$(FUZZBIN) 100000

asan: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
asan: clean test

# clean first so a leftover asan build never runs under valgrind.
# valgrind does not run on Apple Silicon macOS; this target runs in CI
# (.github/workflows/battery.yml, gcc:14 container).
memcheck: clean all
	RB_BIG_N=$(BIG_N) valgrind --leak-check=full --show-leak-kinds=all \
		--error-exitcode=1 ./$(BIN)
	valgrind --leak-check=full --show-leak-kinds=all \
		--error-exitcode=1 ./$(FUZZBIN) 20000

# Local leak check for macOS, where ASan has no LeakSanitizer and valgrind
# is unavailable. A stand-in only; `make memcheck` (CI) is what is graded.
leaks: clean all
	leaks --atExit -- ./$(BIN)
	leaks --atExit -- ./$(FUZZBIN) 20000

clean:
	rm -rf build

.PHONY: all test asan memcheck leaks clean
