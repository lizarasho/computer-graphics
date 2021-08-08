/*
 * File for common macro definitions.
 */

#ifndef LAB7_COMMON_H
#define LAB7_COMMON_H

#define LOGGING_ENABLE 0

#if LOGGING_ENABLE
#define LOG_STDOUT(...) printf(__VA_ARGS__)
#else
#define LOG_STDOUT(...)
#endif

#define LOG_MATRIX_W_H(m, w, h)                          \
    {                                                \
        int u, v;                                    \
        for (u = 0; u < (h); ++u) {                  \
            for (v = 0; v < (w); ++v) {              \
                LOG_STDOUT("%d ", (m)[u * (w) + v]); \
            }                                        \
            LOG_STDOUT("\n");                        \
        }                                                \
    }

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))
#define CLIP(min, val, max) (MIN(MAX(min, val), max))

#define PROCESS_ERROR(...) {      \
    fprintf(stderr, __VA_ARGS__); \
    goto fail;                    \
}

void print_binary(int number, int num_digits) {
    int digit;
    for (digit = num_digits - 1; digit >= 0; digit--) {
        printf("%c", number & (1 << digit) ? '1' : '0');
    }
}

#endif
