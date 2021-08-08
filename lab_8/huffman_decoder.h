#ifndef LAB8_HUFFMAN_DECODER_H
#define LAB8_HUFFMAN_DECODER_H

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

void build_Huffman_tree(const uint8_t *length_to_codes_number, const uint8_t *codes_values, Huffman_node *root) {
    uint16_t cur_code = 0;
    int index = 0;

    int i, j;
    for (i = 0; i < 16; ++i) {
        cur_code <<= 1;
        for (j = 0; j < length_to_codes_number[i]; ++j) {
            uint8_t value = codes_values[index++];
            uint8_t code_length = i + 1;

            Huffman_node *node = (Huffman_node *) malloc(sizeof(Huffman_node));
            init_huffman_node(node, value);
            add_node(root, node, cur_code, code_length);

            ++cur_code;
        }
    }
}

void decode_value_recursively(bitstream_reader *bit_ctx, Huffman_node *node, uint8_t cur_length, uint16_t *result) {
    uint16_t cur_code;
    if (node == NULL) {
        return;
    }
    if ((node->left_node == NULL) && (node->right_node == NULL)) { /* is leaf */
        skip_bits(bit_ctx, cur_length);
        *result = node->value;
        return;
    }
    ++cur_length;
    cur_code = show_bits_16bit(bit_ctx, cur_length);
    if (cur_code & 1) {
        decode_value_recursively(bit_ctx, node->right_node, cur_length, result);
    } else {
        decode_value_recursively(bit_ctx, node->left_node, cur_length, result);
    }
}

void decode_value(bitstream_reader *bit_ctx, Huffman_node *node, uint16_t *result) {
    decode_value_recursively(bit_ctx, node, 0, result);
}

void print_Huffman_tree(Huffman_node *node, uint16_t cur_code, uint8_t cur_length) {
    if (node == NULL) {
        return;
    }
    if (node->left_node == NULL && node->right_node == NULL) { /* is leaf */
        int i;
        LOG_STDOUT("Node %X: ", node->value);
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

#endif
