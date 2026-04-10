#!/bin/zsh
set -euo pipefail

clang++ -std=gnu++03 -I./src tests/startup_rollback_test.cc src/fweelin_startup_guard.cc -o tests/startup_rollback_test
cc -I./src tests/signal_handler_test.c src/fweelin_signal.c -o tests/signal_handler_test
clang++ -std=gnu++03 -I./src tests/root_processor_queue_test.cc src/fweelin_processor_queue.cc -o tests/root_processor_queue_test

./tests/startup_rollback_test
./tests/signal_handler_test
./tests/root_processor_queue_test
