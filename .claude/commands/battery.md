Run make test, make asan, and make leaks in sequence (make memcheck runs in CI; valgrind is unavailable on this Mac).
If any fail, stop and diagnose the first failure: $ARGUMENTS
Show only the failing lines of output, not the passing ones. Do not modify tests to make them pass.
