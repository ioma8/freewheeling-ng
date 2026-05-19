#!/bin/zsh
set -euo pipefail

clang++ -std=gnu++20 -I./src tests/startup_rollback_test.cc src/fweelin_startup_guard.cc -o tests/startup_rollback_test
cc -I./src tests/signal_handler_test.c src/fweelin_signal.c -o tests/signal_handler_test
clang++ -std=gnu++20 -I./src tests/root_processor_queue_test.cc src/fweelin_processor_queue.cc -o tests/root_processor_queue_test
clang++ -std=gnu++20 -I./src tests/event_filename_copy_test.cc -o tests/event_filename_copy_test
clang++ -std=gnu++20 -I./src tests/config_path_expand_test.cc -o tests/config_path_expand_test
clang++ -std=gnu++20 -I./src tests/config_token_split_test.cc -o tests/config_token_split_test
clang++ -std=gnu++20 -I./src tests/saveable_path_builder_test.cc -o tests/saveable_path_builder_test
clang++ -std=gnu++20 -I./src tests/video_scaling_test.cc -o tests/video_scaling_test
clang++ -std=gnu++20 -I./src tests/video_render_metrics_test.cc -o tests/video_render_metrics_test
clang++ -std=gnu++20 -I./src -I./MacOSX -I/opt/homebrew/include -I/opt/homebrew/include/SDL -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk/usr/include/libxml2 tests/video_geometry_scale_test.cc -o tests/video_geometry_scale_test
cc -I./src tests/stacktrace_command_builder_test.c src/stacktrace.c -o tests/stacktrace_command_builder_test
cc -I./src tests/stacktrace_symbol_name_copy_test.c src/stacktrace.c -o tests/stacktrace_symbol_name_copy_test
cc -I./src tests/stacktrace_output_format_test.c src/stacktrace.c -o tests/stacktrace_output_format_test

./tests/startup_rollback_test
./tests/signal_handler_test
./tests/root_processor_queue_test
./tests/event_filename_copy_test
./tests/config_path_expand_test
./tests/config_token_split_test
./tests/saveable_path_builder_test
./tests/video_scaling_test
./tests/video_render_metrics_test
./tests/video_geometry_scale_test
./tests/stacktrace_command_builder_test
./tests/stacktrace_symbol_name_copy_test
./tests/stacktrace_output_format_test
