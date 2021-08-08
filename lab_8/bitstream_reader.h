#ifndef LAB7_BITSTREAM_READER_H
#define LAB7_BITSTREAM_READER_H

#include <string.h>

typedef struct bitstream_reader {
    const uint8_t *buffer;
    uint32_t index;
    uint32_t bits_size;
} bitstream_reader;

void init_bitstream_reader(bitstream_reader *bit_ctx, const uint8_t *buffer, uint32_t bits_size) {
    bit_ctx->buffer = buffer;
    bit_ctx->index = 0;
    bit_ctx->bits_size = bits_size;
}

uint32_t find_next_marker_position(bitstream_reader *bit_ctx) {
#define MIN(a, b) (((a)<(b))?(a):(b))
    uint32_t index;
    assert((bit_ctx->index & 7) == 0);
    for (index = bit_ctx->index; index < bit_ctx->bits_size; index += 8) {
        if (bit_ctx->buffer[index >> 3] == 0xFF && bit_ctx->buffer[(index >> 3) + 1] != 0x00) {
            break;
        }
    }
    return MIN(index, bit_ctx->bits_size);
}

uint32_t get_current_position(bitstream_reader *bit_ctx) {
    return bit_ctx->index;
}

const uint8_t *get_buffer(bitstream_reader *bit_ctx) {
    return bit_ctx->buffer;
}

void copy_from_buffer(bitstream_reader *bit_ctx, uint8_t *dst, uint32_t size) {
    assert((bit_ctx->index & 7) == 0);
    memcpy(dst, bit_ctx->buffer + (bit_ctx->index >> 3), size);
    bit_ctx->index += size << 3;
}

uint8_t show_bits_8bit(bitstream_reader *bit_ctx, int n) {
    uint32_t index = bit_ctx->index;
    uint8_t offset = index & 7;
    uint8_t read_bits = 8 - offset;
    uint8_t result;
    result = bit_ctx->buffer[index >> 3] & ((1 << read_bits) - 1);
    if (read_bits >= n) {
        result >>= read_bits - n;
    } else {
        uint8_t rest_bits_number = n - read_bits;
        uint8_t rest_bits = bit_ctx->buffer[(index >> 3) + 1] >> (8 - rest_bits_number);
        result = (result << rest_bits_number) | rest_bits;
    }
    return result;
}

uint8_t read_bits_8bit(bitstream_reader *bit_ctx, int n) {
    uint8_t result = show_bits_8bit(bit_ctx, n);
    bit_ctx->index += n;
    return result;
}

uint16_t show_bits_16bit(bitstream_reader *bit_ctx, int n) {
    uint16_t result;
    if (n <= 8) {
        result = show_bits_8bit(bit_ctx, n);
    } else {
        uint8_t left_part, right_part;
        left_part = show_bits_8bit(bit_ctx, 8);
        bit_ctx->index += 8;
        right_part = show_bits_8bit(bit_ctx, n - 8);
        bit_ctx->index -= 8;
        result = (left_part << (n - 8)) | right_part;
    }
    return result;
}

uint16_t read_bits_16bit(bitstream_reader *bit_ctx, int n) {
    uint16_t result = show_bits_16bit(bit_ctx, n);
    bit_ctx->index += n;
    return result;
}

void skip_bits(bitstream_reader *bit_ctx, int n) {
    bit_ctx->index += n;
}

#endif