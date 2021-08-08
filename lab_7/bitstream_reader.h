#ifndef LAB7_BITSTREAM_READER_H
#define LAB7_BITSTREAM_READER_H

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

uint32_t get_bits_count(const bitstream_reader *bit_ctx) {
    return bit_ctx->index;
}

uint32_t get_size(const bitstream_reader *bit_ctx) {
    return bit_ctx->bits_size - bit_ctx->index;
}

uint8_t show_bits_8bit_reverse(bitstream_reader *bit_ctx, int n) {
    uint32_t index = bit_ctx->index;
    int offset = index & 7;
    int read_bits = 8 - offset;
    uint8_t result = bit_ctx->buffer[index >> 3] >> offset;
    if (read_bits >= n) {
        result &= (1 << n) - 1;
    } else {
        uint8_t rest_bits = bit_ctx->buffer[(index >> 3) + 1] & ((1 << (n - read_bits)) - 1);
        rest_bits <<= read_bits;
        result |= rest_bits;
    }
    return result;
}

uint8_t reverse_bits_8bit(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

uint8_t show_bits_8bit_direct(bitstream_reader *bit_ctx, int n) {
    uint8_t result = show_bits_8bit_reverse(bit_ctx, n);
    result = reverse_bits_8bit(result);
    result >>= 8 - n;
    return result;
}

uint8_t read_bits_8bit_reverse(bitstream_reader *bit_ctx, int n) {
    uint8_t result = show_bits_8bit_reverse(bit_ctx, n);
    bit_ctx->index += n;
    return result;
}

uint8_t read_bits_8bit_direct(bitstream_reader *bit_ctx, int n) {
    uint8_t result = read_bits_8bit_reverse(bit_ctx, n);
    result = reverse_bits_8bit(result);
    result >>= 8 - n;
    return result;
}

uint16_t read_bits_16bit_direct(bitstream_reader *bit_ctx, int n) {
    uint8_t left_part;
    uint8_t right_part;
    uint16_t result;

    if (n <= 8) {
        result = read_bits_8bit_direct(bit_ctx, n);
    } else {
        left_part = read_bits_8bit_direct(bit_ctx, 8);
        right_part = read_bits_8bit_direct(bit_ctx, n - 8);
        result = (left_part << (n - 8)) | right_part;
    }

    return result;
}

uint16_t read_bits_16bit_reverse(bitstream_reader *bit_ctx, int n) {
    uint8_t left_part;
    uint8_t right_part;
    uint16_t result;

    if (n <= 8) {
        result = read_bits_8bit_reverse(bit_ctx, n);
    } else {
        right_part = read_bits_8bit_reverse(bit_ctx, 8);
        left_part = read_bits_8bit_reverse(bit_ctx, n - 8);
        result = (left_part << 8) | right_part;
    }

    return result;
}

uint16_t show_bits_16bit_direct(bitstream_reader *bit_ctx, int n) {
    uint8_t left_part;
    uint8_t right_part;
    uint16_t result;

    if (n <= 8) {
        result = show_bits_8bit_direct(bit_ctx, n);
    } else {
        left_part = show_bits_8bit_direct(bit_ctx, 8);
        bit_ctx->index += 8;
        right_part = show_bits_8bit_direct(bit_ctx, n - 8);
        bit_ctx->index -= 8;
        result = (left_part << (n - 8)) | right_part;
    }

    return result;
}

#endif
