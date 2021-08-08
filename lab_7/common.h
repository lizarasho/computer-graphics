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

#define PROCESS_ERROR(...) {      \
    fprintf(stderr, __VA_ARGS__); \
    goto fail;                    \
}

// Integers are parsed through bytes arrays because my OS is little-endian, but integers in PNG files are big-endian
#define BYTES_TO_INT(b) (((b)[0] << 24) | ((b)[1] << 16) | ((b)[2] << 8) | (b)[3])

#endif
