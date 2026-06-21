# Native (host) regression tests for the DSP code. This is a standalone CMake
# project built with the host compiler -- do NOT pass the AVR toolchain file.
# The DSP sources are plain integer/fixed-point C with no AVR dependencies, so
# they run identically here and on the target.
#
#   cmake -S test -B test/build
#   cmake --build test/build
#   ctest --test-dir test/build --output-on-failure
cmake_minimum_required(VERSION 3.20)
project(tuna_tests C)

enable_testing()

set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_compile_options(-std=c11 -O2 -Wall -Wextra)

# YIN path (config.h default selects PITCH_METHOD_YIN).
add_executable(test_yin test_yin.c "${SRC}/yin.c")
target_include_directories(test_yin PRIVATE "${SRC}")
target_link_libraries(test_yin m)
add_test(NAME yin COMMAND test_yin)

# FFT path (force PITCH_METHOD_FFT so analysis_fft_frequency is compiled in).
add_executable(test_fft test_fft.c "${SRC}/analysis.c" "${SRC}/fft.c")
target_include_directories(test_fft PRIVATE "${SRC}")
target_compile_definitions(test_fft PRIVATE PITCH_METHOD_FFT)
target_link_libraries(test_fft m)
add_test(NAME fft COMMAND test_fft)

# Window comparison benchmark (FFT path). Reports per-window pitch accuracy and
# guards that the shipped default window stays accurate.
add_executable(bench_window bench_window.c "${SRC}/analysis.c" "${SRC}/fft.c")
target_include_directories(bench_window PRIVATE "${SRC}")
target_compile_definitions(bench_window PRIVATE PITCH_METHOD_FFT)
target_link_libraries(bench_window m)
add_test(NAME window_bench COMMAND bench_window)

# Note mapper (pitch_from_frequency).
add_executable(test_pitch test_pitch.c "${SRC}/pitch.c")
target_include_directories(test_pitch PRIVATE "${SRC}")
target_link_libraries(test_pitch m)
add_test(NAME pitch COMMAND test_pitch)

# Per-frame frequency stabiliser (smooth_frequency).
add_executable(test_smoothing test_smoothing.c "${SRC}/smoothing.c")
target_include_directories(test_smoothing PRIVATE "${SRC}")
target_link_libraries(test_smoothing m)
add_test(NAME smoothing COMMAND test_smoothing)
