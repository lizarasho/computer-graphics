#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "bitstream_reader.h"

#ifndef LAB7_ZLIB_DECODER_H
#define LAB7_ZLIB_DECODER_H

int verify_fcheck_checksum(uint8_t cmf, uint8_t flg, uint8_t actual_fcheck) {
    uint32_t x = (cmf << 8) + flg - actual_fcheck;
    uint32_t y = (x + 30) / 31;
    uint8_t expected_fcheck = y * 31 - x;
    if (actual_fcheck != expected_fcheck) {
        PROCESS_ERROR("Incorrect zlib header structure: expected fcheck %d, found %d.\n",
                      expected_fcheck, actual_fcheck);
    }
    goto end;
    fail:
    return -1;
    end:
    return 0;
}

int parse_zlib_header(bitstream_reader *bit_ctx) {
    uint8_t cmf = read_bits_8bit_reverse(bit_ctx, 8);          // compression method and flags
    uint8_t flg = read_bits_8bit_reverse(bit_ctx, 8);          // flags
    uint8_t cm = cmf & 0x0F;                                      // compression method
    uint8_t cinfo = cmf >> 4;                                     // compression info
    uint8_t fcheck = flg & 0x1F;                                  // check bits for CMF and FLG
    uint8_t fdict = (flg & (1 << 5)) >> 5;                        // preset dictionary
    uint8_t flevel = flg >> 6;                                    // compression level
    LOG_STDOUT("Parsing ZLIB header: CM = %d, CINFO = %d, FCHECK = %d, FDICT = %d, FLEVEL = %d.\n",
               cm, cinfo, fcheck, fdict, flevel);
    if (cm != 8) {
        PROCESS_ERROR("Incorrect zlib header structure: only Deflate compression method (8) is allowed.\n");
    }
    if (verify_fcheck_checksum(cmf, flg, fcheck) < 0) {
        goto fail;
    }

    goto end;

    fail:
    return -1;

    end:
    return 0;
}

uint16_t LENGTHS_TABLE[29][2] = {
        {3,   0}, // 257
        {4,   0}, // 258
        {5,   0}, // 259
        {6,   0}, // 260
        {7,   0}, // 261
        {8,   0}, // 262
        {9,   0}, // 263
        {10,  0}, // 264
        {11,  1}, // 265
        {13,  1}, // 266
        {15,  1}, // 267
        {17,  1}, // 268
        {19,  2}, // 269
        {23,  2}, // 270
        {27,  2}, // 271
        {31,  2}, // 272
        {35,  3}, // 273
        {43,  3}, // 274
        {51,  3}, // 275
        {59,  3}, // 276
        {67,  4}, // 277
        {83,  4}, // 278
        {99,  4}, // 279
        {115, 4}, // 280
        {131, 5}, // 281
        {163, 5}, // 282
        {195, 5}, // 283
        {227, 5}, // 284
        {258, 0}  // 285
};

uint16_t DISTANCES_TABLE[30][2] = {
        {1,     0}, // 0
        {2,     0}, // 1
        {3,     0}, // 2
        {4,     0}, // 3
        {5,     1}, // 4
        {7,     1}, // 5
        {9,     2}, // 6
        {13,    2}, // 7
        {17,    3}, // 8
        {25,    3}, // 9
        {33,    4}, // 10
        {49,    4}, // 11
        {65,    5}, // 12
        {97,    5}, // 13
        {129,   6}, // 14
        {193,   6}, // 15
        {257,   7}, // 16
        {385,   7}, // 17
        {513,   8}, // 18
        {769,   8}, // 19
        {1025,  9}, // 20
        {1537,  9}, // 21
        {2049,  10}, // 22
        {3073,  10}, // 23
        {4097,  11}, // 24
        {6145,  11}, // 25
        {8193,  12}, // 26
        {12289, 12}, // 27
        {16385, 13}, // 28
        {24577, 13}, // 29
};

uint16_t FIXED_HAFFMAN_TABLE[4][5] = {
/* min_code, code_length, diapason_l, prefix_l, prefix_r */
        {0,   8, 0b00110000,  0b0011000, 0b1011111},
        {144, 9, 0b110010000, 0b1100100, 0b1111111},
        {256, 7, 0b0000000,   0b0000000, 0b0010111},
        {280, 8, 0b11000000,  0b1100000, 0b1100011}
};

typedef struct Huffman_node {
    uint16_t value;
    struct Huffman_node *left_node;
    struct Huffman_node *right_node;
} Huffman_node;

void init_huffman_node(Huffman_node *node, uint16_t value) {
    node->value = value;
    node->left_node = NULL;
    node->right_node = NULL;
}

void destroy_huffman_tree(Huffman_node *node) {
    if (node->left_node) {
        destroy_huffman_tree(node->left_node);
    }
    if (node->right_node) {
        destroy_huffman_tree(node->right_node);
    }
    free(node);
}

int decode_Huffman_code(bitstream_reader *bit_ctx, uint8_t *resulting_values, int *read_values,
                        uint16_t value, Huffman_node *alphabet,
                        void (*decode_offset_value)(bitstream_reader *, Huffman_node *, uint16_t *)) {
    *read_values = 0;

    if (value > 256) {
        uint16_t length_start = LENGTHS_TABLE[value - 257][0];
        uint16_t length_add_bits_number = LENGTHS_TABLE[value - 257][1];
        uint16_t length_shift = read_bits_8bit_reverse(bit_ctx, length_add_bits_number);
        uint16_t length_value = length_start + length_shift;

        uint16_t offset_value;
        decode_offset_value(bit_ctx, alphabet, &offset_value);
        uint16_t offset_distance_start = DISTANCES_TABLE[offset_value][0];
        uint16_t offset_add_bits_number = DISTANCES_TABLE[offset_value][1];
        uint16_t offset_distance_shift = read_bits_16bit_reverse(bit_ctx, offset_add_bits_number);
        uint16_t offset_distance_value = offset_distance_start + offset_distance_shift;

        uint8_t *copied_values = resulting_values - offset_distance_value;

        LOG_STDOUT("Read values");
        while (*read_values < length_value) {
            resulting_values[*read_values] = copied_values[*read_values % offset_distance_value];
            LOG_STDOUT(" %d", resulting_values[*read_values]);
            *read_values += 1;
        }
        LOG_STDOUT(" with length = %d, offset = %d.\n", length_value, offset_distance_value);

        return 0;
    } else if (value == 256) {
        LOG_STDOUT("END.\n");
        return 1;
    } else { /* value < 256 */
        LOG_STDOUT("Read value %d.\n", value);
        *resulting_values = value;
        *read_values = 1;
        return 0;
    }
}

void decode_fixed_offset(bitstream_reader *bit_ctx, Huffman_node *alphabet, uint16_t *offset_value) {
    *offset_value = read_bits_8bit_direct(bit_ctx, 5);
}

int decode_fixed_Huffman_value(bitstream_reader *bit_ctx, uint8_t *resulting_values, int *read_values) {
    uint16_t prefix;
    uint16_t value;
    uint16_t code_length, min_code, diapason_l;
    int type;
    int i;
    prefix = show_bits_8bit_direct(bit_ctx, 7);

    type = 0;
    for (i = 1; i <= 4; ++i) {
        uint16_t prefix_l = FIXED_HAFFMAN_TABLE[i - 1][3];
        uint16_t prefix_r = FIXED_HAFFMAN_TABLE[i - 1][4];
        if (prefix >= prefix_l && prefix <= prefix_r) {
            type = i;
            break;
        }
    }

    code_length = FIXED_HAFFMAN_TABLE[type - 1][1];
    min_code = FIXED_HAFFMAN_TABLE[type - 1][0];
    diapason_l = FIXED_HAFFMAN_TABLE[type - 1][2];
    value = read_bits_16bit_direct(bit_ctx, code_length);
    value = value - diapason_l + min_code;

    return decode_Huffman_code(bit_ctx, resulting_values, read_values, value, NULL, &decode_fixed_offset);
}

int decode_fixed_Huffman_code(bitstream_reader *bit_ctx, uint8_t *values) {
    int i = 0;
    int end = 0;
    while (!end) {
        int read_values;
        end = decode_fixed_Huffman_value(bit_ctx, values + i, &read_values);
        i += read_values;
    }
    return i;
}

uint8_t COMMANDS_ORDER[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

void add_node(Huffman_node *root, Huffman_node *node, uint16_t code_value, uint8_t code_length) {
    int i;
    Huffman_node *cur_node = root;
    for (i = code_length - 1; i >= 0; --i) {
        uint8_t cur_bit = (code_value & (1 << i)) >> i;
        if (i != 0) { /* Current node is not leaf */
            if (cur_bit == 1) { /* Go to right */
                if (cur_node->right_node) {
                    cur_node = cur_node->right_node;
                } else { /* Create new right node */
                    Huffman_node *right_node = (Huffman_node *) malloc(sizeof(Huffman_node));
                    init_huffman_node(right_node, 0);
                    cur_node->right_node = right_node;
                    cur_node = right_node;
                }
            } else { /* Go to left */
                if (cur_node->left_node) {
                    cur_node = cur_node->left_node;
                } else { /* Create new left node */
                    Huffman_node *left_node = (Huffman_node *) malloc(sizeof(Huffman_node));
                    init_huffman_node(left_node, 0);
                    cur_node->left_node = left_node;
                    cur_node = left_node;
                }
            }
        } else { /* Current node is leaf, hang the node */
            if (cur_bit == 1) {
                cur_node->right_node = node;
            } else {
                cur_node->left_node = node;
            }
        }
    }
}

void print_Huffman_tree(Huffman_node *node, uint16_t cur_code, uint8_t cur_length) {
    if (node == NULL) {
        return;
    }
    if (node->left_node == NULL && node->right_node == NULL) { /* is leaf */
        int i;
        LOG_STDOUT("Node %d: ", node->value);
        for (i = cur_length - 1; i >= 0; i--) {
            LOG_STDOUT("%c", cur_code & (1 << i) ? '1' : '0');
        }
        LOG_STDOUT("\n");
        return;
    }
    cur_code <<= 1;
    print_Huffman_tree(node->left_node, cur_code | 0, cur_length + 1);
    print_Huffman_tree(node->right_node, cur_code | 1, cur_length + 1);
}

void decode_command_recursively(bitstream_reader *bit_ctx, Huffman_node *node, uint8_t cur_length, uint16_t *result) {
    uint16_t cur_code;
    if (node == NULL) {
        return;
    }
    if ((node->left_node == NULL) && (node->right_node == NULL)) { /* is leaf */
        read_bits_16bit_direct(bit_ctx, cur_length);
        *result = node->value;
        return;
    }
    ++cur_length;
    cur_code = show_bits_16bit_direct(bit_ctx, cur_length);
    if (cur_code & 1) {
        decode_command_recursively(bit_ctx, node->right_node, cur_length, result);
    } else {
        decode_command_recursively(bit_ctx, node->left_node, cur_length, result);
    }
}

void decode_command(bitstream_reader *bit_ctx, Huffman_node *node, uint16_t *result) {
    decode_command_recursively(bit_ctx, node, 0, result);
}

#define INF UINT16_MAX

void build_Huffman_tree(uint16_t **length_to_codes, int rows, Huffman_node *root) {
    int i, j;
    uint16_t cur_code = 0;
    for (i = 0; i < rows; ++i) {
        cur_code <<= 1;
        if (length_to_codes[i][0] == INF) {
            continue;
        }

        j = 0;
        while (length_to_codes[i][j] != INF) {
            uint16_t command = length_to_codes[i][j];
            uint8_t code_length = i;

            Huffman_node *node = (Huffman_node *) malloc(sizeof(Huffman_node));
            init_huffman_node(node, command);
            add_node(root, node, cur_code, code_length);

            ++cur_code;
            ++j;
        }
    }
}

void init_arrays(uint16_t **length_to_values, uint8_t *value_to_length, uint16_t values_max, uint8_t max_code_length) {
    int i, j;
    for (i = 0; i < values_max; ++i) {
        value_to_length[i] = 0;
        for (j = 0; j < max_code_length; ++j) {
            length_to_values[j][i] = INF;
        }
    }
}

void fill_reverse_array(uint16_t values_max, const uint8_t *value_to_length, uint16_t **length_to_values) {
    int i;
    for (i = 0; i < values_max; ++i) {
        if (value_to_length[i] != 0) {
            uint8_t length = value_to_length[i];
            int j = 0;
            while (length_to_values[length][j] != INF) {
                ++j;
            }
            length_to_values[length][j] = i;
        }
    }
}

void decode_commands_alphabet(bitstream_reader *bit_ctx, uint8_t hclen, Huffman_node *commands_alphabet) {
    const uint8_t commands_max = 19; /* Max number of commands */
    const uint8_t max_code_length = 20; /* Max length of the resulting code */

    uint8_t alphabet_size = hclen + 4;
    uint8_t command_to_length[commands_max];
    uint16_t **length_to_commands;

    int i;

    length_to_commands = (uint16_t **) malloc(max_code_length * sizeof(uint16_t *));
    for (i = 0; i < max_code_length; ++i) {
        length_to_commands[i] = (uint16_t *) malloc(commands_max * sizeof(uint16_t));
    }
    init_arrays(length_to_commands, command_to_length, commands_max, max_code_length);

    for (i = 0; i < alphabet_size; ++i) {
        uint8_t command = COMMANDS_ORDER[i];
        uint8_t length = read_bits_8bit_reverse(bit_ctx, 3);
        command_to_length[command] = length;
    }

    fill_reverse_array(commands_max, command_to_length, length_to_commands);

    build_Huffman_tree(length_to_commands, max_code_length, commands_alphabet);

    for (i = 0; i < max_code_length; ++i) {
        free(length_to_commands[i]);
    }
    free(length_to_commands);
}

void decode_commands_sequence(bitstream_reader *bit_ctx, uint16_t values_number, uint8_t *value_to_length,
                              Huffman_node *command_alphabet) {
    int i = 0;
    uint16_t prev_command;
    while (i < values_number) {
        uint16_t command;
        uint8_t repeats;
        uint8_t repeated_value;
        decode_command(bit_ctx, command_alphabet, &command);
        repeats = 0;
        if (command == 16) {
            repeats = 3 + read_bits_8bit_reverse(bit_ctx, 2);
            repeated_value = prev_command;
        } else if (command == 17) {
            repeats = 3 + read_bits_8bit_reverse(bit_ctx, 3);
            repeated_value = 0;
        } else if (command == 18) {
            repeats = 11 + read_bits_8bit_reverse(bit_ctx, 7);
            repeated_value = 0;
        }
        LOG_STDOUT("Decoded command: %d, repeats: %d\n", command, repeats);
        if (repeats) {
            int j;
            for (j = 0; j < repeats; ++j) {
                value_to_length[i++] = repeated_value;
            }
            command = repeated_value;
        } else {
            value_to_length[i++] = command;
        }
        prev_command = command;
    }
}

/* Decode alphabet for symbols and lengths and alphabet for distances */
void decode_basic_alphabets(bitstream_reader *bit_ctx, uint8_t hlit, uint8_t hdist, Huffman_node *command_alphabet,
                            Huffman_node *symbols_and_lengths_alphabet, Huffman_node *distances_alphabet) {

    const uint16_t symbols_and_lengths_max = 32 + 257; /* Max number of symbols and lengths */
    const uint8_t distances_max = 32 + 1; /* Max number of distances */
    const uint8_t max_code_length = 20; /* Max length of the resulting code */

    uint16_t symbols_and_lengths_number = hlit + 257;
    uint8_t symbol_or_length_to_length[symbols_and_lengths_max];
    uint16_t **length_to_symbols_and_lengths;

    uint8_t distances_number = hdist + 1;
    uint8_t distance_to_length[distances_max];
    uint16_t **length_to_distances;

    int i;

    length_to_symbols_and_lengths = (uint16_t **) malloc(max_code_length * sizeof(uint16_t *));
    length_to_distances = (uint16_t **) malloc(max_code_length * sizeof(uint16_t *));
    for (i = 0; i < max_code_length; ++i) {
        length_to_symbols_and_lengths[i] = (uint16_t *) malloc(symbols_and_lengths_max * sizeof(uint16_t));
        length_to_distances[i] = (uint16_t *) malloc(distances_max * sizeof(uint16_t));
    }
    init_arrays(length_to_symbols_and_lengths, symbol_or_length_to_length, symbols_and_lengths_max, max_code_length);
    init_arrays(length_to_distances, distance_to_length, distances_max, max_code_length);

    LOG_STDOUT("Started decoding symbols-and-lengths alphabets.\n");
    decode_commands_sequence(bit_ctx, symbols_and_lengths_number, symbol_or_length_to_length, command_alphabet);
    LOG_STDOUT("\n");

    LOG_STDOUT("Started decoding distances alphabets.\n");
    decode_commands_sequence(bit_ctx, distances_number, distance_to_length, command_alphabet);
    LOG_STDOUT("\n");

    fill_reverse_array(symbols_and_lengths_max, symbol_or_length_to_length, length_to_symbols_and_lengths);
    fill_reverse_array(distances_max, distance_to_length, length_to_distances);

    build_Huffman_tree(length_to_symbols_and_lengths, max_code_length, symbols_and_lengths_alphabet);
    build_Huffman_tree(length_to_distances, max_code_length, distances_alphabet);

    for (i = 0; i < max_code_length; ++i) {
        free(length_to_distances[i]);
        free(length_to_symbols_and_lengths[i]);
    }
    free(length_to_distances);
    free(length_to_symbols_and_lengths);
}

int decode_dynamic_Huffman_value(bitstream_reader *bit_ctx, uint8_t *resulting_values, int *read_values,
                                 Huffman_node *symbols_and_lengths_alphabet, Huffman_node *distances_alphabet) {
    uint16_t value;
    decode_command(bit_ctx, symbols_and_lengths_alphabet, &value);
    return decode_Huffman_code(bit_ctx, resulting_values, read_values, value, distances_alphabet, &decode_command);
}

int decode_dynamic_Huffman_code(bitstream_reader *bit_ctx, uint8_t *values) {
#define PRINT_HUFFMAN_TREE(alphabet, s) \
    LOG_STDOUT("Resulting Huffman tree for %s alphabet:\n", s); \
    print_Huffman_tree(alphabet, 0, 0); \
    LOG_STDOUT("\n");

    uint8_t hlit = read_bits_8bit_reverse(bit_ctx, 5);
    uint8_t hdist = read_bits_8bit_reverse(bit_ctx, 5);
    uint8_t hclen = read_bits_8bit_reverse(bit_ctx, 4);
    Huffman_node *commands_alphabet;
    Huffman_node *symbols_and_lengths_alphabet;
    Huffman_node *distances_alphabet;

    int i;
    int end;

    LOG_STDOUT("HLIT: %d, HDIST: %d, HCLEN: %d.\n\n", hlit, hdist, hclen);

    commands_alphabet = (Huffman_node *) malloc(sizeof(Huffman_node));
    symbols_and_lengths_alphabet = (Huffman_node *) malloc(sizeof(Huffman_node));
    distances_alphabet = (Huffman_node *) malloc(sizeof(Huffman_node));
    init_huffman_node(commands_alphabet, 0);
    init_huffman_node(symbols_and_lengths_alphabet, 0);
    init_huffman_node(distances_alphabet, 0);

    decode_commands_alphabet(bit_ctx, hclen, commands_alphabet);

    PRINT_HUFFMAN_TREE(commands_alphabet, "commands");

    decode_basic_alphabets(bit_ctx, hlit, hdist, commands_alphabet, symbols_and_lengths_alphabet, distances_alphabet);

    PRINT_HUFFMAN_TREE(symbols_and_lengths_alphabet, "symbols-and-lengths");
    PRINT_HUFFMAN_TREE(distances_alphabet,
                       "distances");

    i = 0;
    end = 0;
    while (!end) {
        int read_values;
        end = decode_dynamic_Huffman_value(bit_ctx, values + i, &read_values,
                                           symbols_and_lengths_alphabet, distances_alphabet);
        i += read_values;
    }

    destroy_huffman_tree(commands_alphabet);
    destroy_huffman_tree(symbols_and_lengths_alphabet);
    destroy_huffman_tree(distances_alphabet);

    return i;
}

int decode_no_compression(bitstream_reader *bit_ctx, uint8_t *values) {
    uint16_t len;
    int i;

    /* Skip remaining bits */
    read_bits_8bit_direct(bit_ctx, 8 - bit_ctx->index % 8);

    len = read_bits_16bit_reverse(bit_ctx, 16);
    LOG_STDOUT("LEN = %d.\n", len);

    /* Skip NLEN */
    read_bits_16bit_reverse(bit_ctx, 16);

    for (i = 0; i < len; ++i) {
        values[i] = read_bits_8bit_reverse(bit_ctx, 8);
    }
    return len;
}

uint8_t Paeth_predictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    if (pa <= pb && pa <= pc) {
        return a;
    }
    if (pb <= pc) {
        return b;
    }
    return c;
}

void remove_filtering(const uint8_t *filtered_values, uint8_t *values, int width, int height, int channels) {
    int i, j, k;
    int bpp = channels;

    LOG_STDOUT("Started reversing the effect of a filter.\n");

    for (i = 0; i < height; ++i) {
        uint8_t filter_type = filtered_values[i * width * channels + i];
        LOG_STDOUT("Line %d processing: filter type = %d.\n", i + 1, filter_type);
        for (j = 0; j < width; ++j) {
            for (k = 0; k < channels; ++k) {
                int index = i * width * channels + j * channels + k;

#define ORIG_VAL (filtered_values[index + i + 1])
#define LEFT_VAL ((j == 0) ? 0 : values[index - bpp])
#define PRIOR_VAL ((i == 0) ? 0 : values[index - width * channels])
#define PRIOR_LEFT_VAL ((i == 0 || j == 0) ? 0 : values[index - width * channels - bpp])

                if (filter_type == 0) {        /* None */
                    values[index] = ORIG_VAL;
                } else if (filter_type == 1) { /* Sub */
                    values[index] = ORIG_VAL + LEFT_VAL;
                } else if (filter_type == 2) { /* Up */
                    values[index] = ORIG_VAL + PRIOR_VAL;
                } else if (filter_type == 3) { /* Average */
                    values[index] = ORIG_VAL + floor(((double) LEFT_VAL + PRIOR_VAL) / 2.0);
                } else if (filter_type == 4) { /* Paeth */
                    values[index] = ORIG_VAL + Paeth_predictor(LEFT_VAL, PRIOR_VAL, PRIOR_LEFT_VAL);
                }
            }
        }
    }
    LOG_STDOUT("Finished filtering.\n");
}

int decode_data(bitstream_reader *bit_ctx, int width, int height, int channels, uint8_t *output_values) {
    uint8_t bfinal;
    uint8_t btype;
    int full_size;
    uint8_t *filtered_values;
    int values_ptr;

    full_size = width * height * channels + height;
    filtered_values = (uint8_t *) malloc(full_size);

    bfinal = 0;
    values_ptr = 0;

    while (bfinal != 1) {
        bfinal = read_bits_8bit_reverse(bit_ctx, 1);
        btype = read_bits_8bit_reverse(bit_ctx, 2);

        LOG_STDOUT("Started data decompressing: BFINAL = %d, BTYPE = %d.\n", bfinal, btype);

        switch (btype) {
            case 0: {
                LOG_STDOUT("No compression.\n");
                values_ptr += decode_no_compression(bit_ctx, filtered_values + values_ptr);
                break;
            }
            case 1: {
                LOG_STDOUT("Fixed Huffman codes.\n");
                values_ptr += decode_fixed_Huffman_code(bit_ctx, filtered_values + values_ptr);
                break;
            }
            case 2: {
                LOG_STDOUT("Dynamic Huffman codes.\n");
                LOG_STDOUT("values_ptr: %d.\n", values_ptr);
                values_ptr += decode_dynamic_Huffman_code(bit_ctx, filtered_values + values_ptr);
                break;
            }
            default: {
                PROCESS_ERROR("Invalid BTYPE value %d. Must be from 0 to 2.\n", btype);
            }
        }
    }
    remove_filtering(filtered_values, output_values, width, height, channels);

    goto end;

    fail:
    return -1;

    end:
    free(filtered_values);
    return 0;
}

#endif
