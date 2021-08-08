#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include "common.h"
#include "bitstream_reader.h"
#include "huffman_decoder.h"
#include "upscale.h"

#define MB_W 8
#define MB_H 8
#define MB_SQUARE (MB_W * MB_H)

static const uint8_t zig_zag[MB_SQUARE] = {
        0, 1, 5, 6, 14, 15, 27, 28,
        2, 4, 7, 13, 16, 26, 29, 42,
        3, 8, 12, 17, 25, 30, 41, 43,
        9, 11, 18, 24, 31, 40, 44, 53,
        10, 19, 23, 32, 39, 45, 52, 54,
        20, 22, 33, 38, 46, 51, 55, 60,
        21, 34, 37, 47, 50, 56, 59, 61,
        35, 36, 48, 49, 57, 58, 62, 63,
};

static const uint8_t reverse_zig_zag[MB_SQUARE] = {
        0, 1, 8, 16, 9, 2, 3, 10,
        17, 24, 32, 25, 18, 11, 4, 5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13, 6, 7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63
};

typedef struct frame_component {
    uint8_t id;
    uint8_t H; // horizontal factor
    uint8_t V; // vertical factor
    uint8_t quant_matrix_id;
    uint8_t table_id_DC;
    uint8_t table_id_AC;
    int *mb_input_data;
    uint8_t *output_data;
} frame_component;

#define SOI 0xFFD8
#define DHT 0xFFC4
#define DQT 0xFFDB
#define DRI 0xFFDD
#define SOS 0xFFDA
#define COM 0xFFFE
#define EOI 0xFFD9
#define SOF0 0xFFC0

#define APPn_MASK 0xFFE0

uint16_t frame_height;
uint16_t frame_width;

uint8_t **quant_matrices;

frame_component *components;
uint8_t components_number;
uint8_t H_max;
uint8_t V_max;

Huffman_node *huffman_trees_AC;
Huffman_node *huffman_trees_DC;

bitstream_reader reader;

uint8_t *output_data;

#define LOG_MATRIX(m) LOG_MATRIX_W_H(m, MB_W, MB_H)

void parse_DQT() {
    uint16_t length;
    uint16_t read_bytes;

    length = read_bits_16bit(&reader, 16);
    LOG_STDOUT("Length = %d.\n", length);

    read_bytes = 2;
    while (read_bytes < length) {
        uint8_t *matrix_buffer;
        uint8_t precision, matrix_id;

        int i;

        precision = read_bits_8bit(&reader, 4);
        matrix_id = read_bits_8bit(&reader, 4);
        ++read_bytes;

        assert(precision == 0);
        assert(matrix_id >= 0 && matrix_id <= 3);

        matrix_buffer = (uint8_t *) malloc(MB_SQUARE * sizeof(uint8_t));
        copy_from_buffer(&reader, matrix_buffer, MB_SQUARE);
        read_bytes += MB_SQUARE;

        for (i = 0; i < MB_SQUARE; ++i) {
            quant_matrices[matrix_id][i] = matrix_buffer[zig_zag[i]];
        }
        LOG_STDOUT("Read dequantization matrix with id = %d:\n", matrix_id);
        LOG_MATRIX(quant_matrices[matrix_id])

        free(matrix_buffer);
    }
}

void parse_SOF0() {
    uint16_t length;
    uint8_t precision;
    uint16_t MCU_width;
    uint16_t MCU_height;

    int i;

    length = read_bits_16bit(&reader, 16);
    LOG_STDOUT("Length = %d.\n", length);

    precision = read_bits_8bit(&reader, 8);
    assert(precision == 8);

    frame_height = read_bits_16bit(&reader, 16);
    frame_width = read_bits_16bit(&reader, 16);
    LOG_STDOUT("Width = %d, Height = %d.\n", frame_width, frame_height);

    components_number = read_bits_8bit(&reader, 8);
    assert(components_number == 1 || components_number == 3);

    output_data = (uint8_t *) malloc(components_number * frame_width * frame_height * sizeof(uint8_t));

    components = (frame_component *) malloc(components_number * sizeof(frame_component));
    H_max = V_max = 0;

    for (i = 0; i < components_number; ++i) {
        uint8_t H, V;
        components[i].id = read_bits_8bit(&reader, 8);
        H = read_bits_8bit(&reader, 4);
        V = read_bits_8bit(&reader, 4);
        assert(H >= 0 && H <= 4);
        assert(V >= 0 && V <= 4);
        H_max = MAX(H_max, H);
        V_max = MAX(V_max, V);
        components[i].H = H;
        components[i].V = V;
        components[i].quant_matrix_id = read_bits_8bit(&reader, 8);
        LOG_STDOUT("Read %d-th component info: id = %d, H = %d, V = %d, quant_matrix_id = %d.\n",
                   i + 1, components[i].id, H, V, components[i].quant_matrix_id);
    }

    MCU_height = V_max * MB_H;
    MCU_width = H_max * MB_W;
    assert(frame_width % MCU_width == 0 && frame_height % MCU_height == 0);
}

void parse_DHT() {
    uint16_t length;
    uint16_t read_bytes;

    length = read_bits_16bit(&reader, 16);
    LOG_STDOUT("Length = %d.\n", length);

    read_bytes = 2;
    while (read_bytes < length) {
        uint8_t length_to_codes_number[16];
        uint8_t table_class;
        uint8_t table_id;
        int i;
        int codes_number;
        uint8_t *codes_values;
        Huffman_node *root;

        table_class = read_bits_8bit(&reader, 4);
        table_id = read_bits_8bit(&reader, 4);
        ++read_bytes;

        copy_from_buffer(&reader, length_to_codes_number, 16);
        read_bytes += 16;

        LOG_STDOUT("Read %s Huffman Table with id %d:\n", table_class ? "AC" : "DC", table_id);
        LOG_STDOUT("Code-Length - Number-of-codes_number: ");
        codes_number = 0;
        for (i = 0; i < 16; ++i) {
            LOG_STDOUT("%d-%d ", i + 1, length_to_codes_number[i]);
            codes_number += length_to_codes_number[i];
        }
        LOG_STDOUT("\n");

        codes_values = (uint8_t *) malloc(codes_number * sizeof(uint8_t));
        copy_from_buffer(&reader, codes_values, codes_number);
        read_bytes += codes_number;

        LOG_STDOUT("Codes-Values: ");
        for (i = 0; i < codes_number; ++i) {
            LOG_STDOUT("%X ", codes_values[i]);
        }
        LOG_STDOUT("\n");

        if (table_class == 1) {
            root = &huffman_trees_AC[table_id];
        } else {
            root = &huffman_trees_DC[table_id];
        }
        build_Huffman_tree(length_to_codes_number, codes_values, root);

        LOG_STDOUT("Resulting Huffman Tree:\n");
        print_Huffman_tree(root, 0, 0);
        LOG_STDOUT("\n");

        free(codes_values);
    }
}

uint8_t *filter_scan_data(uint32_t *new_buffer_size, uint32_t *full_size) {
    uint32_t cur_index = get_current_position(&reader);
    uint32_t end_index = find_next_marker_position(&reader);
    const uint8_t *buffer = get_buffer(&reader);
    uint8_t *new_buffer;
    size_t FF00_number;
    uint32_t i;

    assert((cur_index & 7) == 0);
    assert((end_index & 7) == 0);
    FF00_number = 0;
    for (i = cur_index; i < end_index; i += 8) {
        if (buffer[i >> 3] == 0xFF && buffer[(i >> 3) + 1] == 0x00) {
            ++FF00_number;
        }
    }
    *full_size = (end_index - cur_index) >> 3;
    *new_buffer_size = *full_size - FF00_number;
    new_buffer = (uint8_t *) malloc(*new_buffer_size * sizeof(uint8_t));
    for (i = 0; i < *new_buffer_size; ++i) {
        new_buffer[i] = buffer[cur_index >> 3];
        if (buffer[cur_index >> 3] == 0xFF && buffer[(cur_index >> 3) + 1] == 0x00) {
            cur_index += 8;
        }
        cur_index += 8;
    }
    return new_buffer;
}

void fill_zeros(int *data_buffer, int l, int r) {
    int i;
    for (i = l; i < r; ++i) {
        data_buffer[reverse_zig_zag[i]] = 0;
    }
}

void decode_macroblock(bitstream_reader *scan_reader, int *data_buffer,
                       Huffman_node *huffman_tree_DC, Huffman_node *huffman_tree_AC) {
    uint16_t DC_length;
    int DC_value;
    int i;
    decode_value(scan_reader, huffman_tree_DC, &DC_length);
    if (DC_length == 0) {
        DC_value = 0;
    } else {
        uint8_t first_digit;
        first_digit = show_bits_8bit(scan_reader, 1);
        DC_value = read_bits_16bit(scan_reader, DC_length);
        if (first_digit == 0) {
            DC_value = DC_value - (1 << DC_length) + 1;
        }
    }
    data_buffer[0] = DC_value;

    i = 1;
    while (i < MB_SQUARE) {
        int AC_value;
        uint8_t zeros;
        uint8_t AC_length;
        uint8_t first_digit;
        uint16_t x;

        decode_value(scan_reader, huffman_tree_AC, &x);
        if (x == 0) {
            fill_zeros(data_buffer, i, MB_SQUARE);
            break;
        }
        zeros = (x & 0xF0) >> 4;
        AC_length = x & 0x0F;

        fill_zeros(data_buffer, i, i + zeros + 1);
        i += zeros;

        first_digit = show_bits_8bit(scan_reader, 1);
        AC_value = read_bits_16bit(scan_reader, AC_length);
        if (first_digit == 0) {
            AC_value = AC_value - (1 << AC_length) + 1;
        }
        data_buffer[reverse_zig_zag[i++]] = AC_value;
    }
}

void dequantization(int *mb_data, const uint8_t *quant_matrix) {
    int k;
    for (k = 0; k < MB_SQUARE; ++k) {
        mb_data[k] *= quant_matrix[k];
    }
}

static double IDCT_TABLE[MB_H][MB_W];

void fill_IDCT_table() {
    int i, j;
    for (i = 0; i < MB_H; ++i) {
        for (j = 0; j < MB_W; ++j) {
            double C_i;
            if (i == 0) {
                C_i = sqrt(2) / 2.;
            } else {
                C_i = 1.;
            }
            IDCT_TABLE[i][j] = C_i * cos(((2 * j + 1) * i * M_PI) / 16);
        }
    }
}

void IDCT(int *matrix) {
    int x, y;
    size_t data_size = MB_SQUARE * sizeof(int);
    int *buffer = (int *) malloc(data_size);

    for (x = 0; x < MB_W; ++x) {
        for (y = 0; y < MB_H; ++y) {
            int u, v;
            double sum = 0;
            for (u = 0; u < MB_W; ++u) {
                for (v = 0; v < MB_H; ++v) {
                    sum += matrix[v * 8 + u] * IDCT_TABLE[u][x] * IDCT_TABLE[v][y];
                }
            }
            buffer[y * 8 + x] = (int) (sum / 4.);
        }
    }

    memcpy(matrix, buffer, data_size);

    free(buffer);
}

void YCbCr_to_RGB() {
    uint8_t *Y_values = components[0].output_data;
    uint8_t *Cb_values = components[1].output_data;
    uint8_t *Cr_values = components[2].output_data;
    int i, j;
    for (i = 0; i < frame_height; ++i) {
        for (j = 0; j < frame_width; ++j) {
            int in_index = i * frame_width + j;
            int out_index = i * components_number * frame_width + j * components_number;
            uint8_t Y = Y_values[in_index];
            uint8_t Cb = Cb_values[in_index];
            uint8_t Cr = Cr_values[in_index];
            int R = (int) round(Y + 1.402 * (Cr - 128));
            int G = (int) round(Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128));
            int B = (int) round(Y + 1.772 * (Cb - 128));
            output_data[out_index] = (uint8_t) CLIP(0, R, 255);
            output_data[out_index + 1] = (uint8_t) CLIP(0, G, 255);
            output_data[out_index + 2] = (uint8_t) CLIP(0, B, 255);
        }
    }
}

void decode_scan_data() {
    uint32_t new_size, full_new_size;
    uint8_t *new_buffer = filter_scan_data(&new_size, &full_new_size);
    bitstream_reader scan_reader;
    uint16_t horizontal_MCU_number;
    uint16_t vertical_MCU_number;
    int prev_DC[components_number];
    int i, j, k;

    LOG_STDOUT("Started decoding scan data.\n");
    init_bitstream_reader(&scan_reader, new_buffer, new_size << 3);
    skip_bits(&reader, full_new_size << 3);
    fill_IDCT_table();

    horizontal_MCU_number = (frame_width / MB_W) / H_max;
    vertical_MCU_number = (frame_height / MB_H) / V_max;

    for (k = 0; k < components_number; ++k) {
        uint16_t width_k = frame_width * components[k].H / H_max;
        uint16_t height_k = frame_height * components[k].V / V_max;
        components[k].mb_input_data = (int *) malloc(width_k * height_k * sizeof(int));
        components[k].output_data = (uint8_t *) malloc(frame_width * frame_height * sizeof(uint8_t));
        prev_DC[k] = 0;
    }

    // Decode interleaved data
    for (i = 0; i < vertical_MCU_number; ++i) {
        for (j = 0; j < horizontal_MCU_number; ++j) {
            LOG_STDOUT("\n*** Processing (%d, %d) MCU ***\n", i, j);
            for (k = 0; k < components_number; ++k) {
                uint8_t table_id_DC = components[k].table_id_DC;
                uint8_t table_id_AC = components[k].table_id_AC;
                Huffman_node huffman_tree_DC = huffman_trees_DC[table_id_DC];
                Huffman_node huffman_tree_AC = huffman_trees_AC[table_id_AC];
                uint8_t *quant_matrix = quant_matrices[components[k].quant_matrix_id];

                int left_corner = (horizontal_MCU_number * i + j) * components[k].H * components[k].V * MB_SQUARE;
                int mb_i, mb_j;

                LOG_STDOUT("\n***** Decoding component %d scan *****\n", components[k].id);
                for (mb_i = 0; mb_i < components[k].V; ++mb_i) {
                    for (mb_j = 0; mb_j < components[k].H; ++mb_j) {
                        int *mb_data =
                                components[k].mb_input_data + left_corner + (components[k].H * mb_i + mb_j) * MB_SQUARE;

                        decode_macroblock(&scan_reader, mb_data, &huffman_tree_DC, &huffman_tree_AC);

                        mb_data[0] += prev_DC[k]; /* Add the previous DC coefficient */
                        prev_DC[k] = mb_data[0];

                        dequantization(mb_data, quant_matrix);

                        IDCT(mb_data);

                        LOG_MATRIX(mb_data)
                        LOG_STDOUT("\n");
                    }
                }
            }
        }
    }

    for (k = 0; k < components_number; ++k) {
        uint16_t width_k = frame_width * components[k].H / H_max;
        uint16_t height_k = frame_height * components[k].V / V_max;

        uint8_t *tmp_buffer = (uint8_t *) calloc(width_k * height_k, sizeof(uint8_t));

        int mb_i, mb_j;

        for (i = 0; i < vertical_MCU_number * horizontal_MCU_number; ++i) {
            int w = i % horizontal_MCU_number;
            int h = i / horizontal_MCU_number;

            for (mb_i = 0; mb_i < components[k].V; ++mb_i) {
                for (mb_j = 0; mb_j < components[k].H; ++mb_j) {
                    int left_corner = i * components[k].H * components[k].V * MB_SQUARE;
                    int *mb_data =
                            components[k].mb_input_data + left_corner + (components[k].H * mb_i + mb_j) * MB_SQUARE;
                    int mb_w = (w * components[k].H + mb_j) * MB_W;
                    int mb_h = (h * components[k].V + mb_i) * MB_H;

                    int p;
                    for (p = 0; p < MB_H; ++p) {
                        int out_w = mb_w;
                        int out_h = mb_h + p;
                        int *src = mb_data + MB_W * p;
                        uint8_t *dst = tmp_buffer + out_h * width_k + out_w;
                        int l;
                        for (l = 0; l < MB_W; ++l) {
                            dst[l] = CLIP(0, src[l] + 128, 255);
                        }
                    }
                }
            }
        }

        if (frame_height > height_k || frame_width > width_k) {
            upscale(tmp_buffer, width_k, height_k, components[k].output_data, frame_width, frame_height);
        } else {
            memcpy(components[k].output_data, tmp_buffer, frame_width * frame_height);
        }
        LOG_STDOUT("Full data for %d-th component.\n", components[k].id);
        LOG_MATRIX_W_H(components[k].output_data, frame_width, frame_height)
        LOG_STDOUT("\n");

        free(tmp_buffer);
    }

    if (components_number == 3) {
        YCbCr_to_RGB();
    } else {
        /* Grayscale */
        memcpy(output_data, components[0].output_data, frame_width * frame_height);
    }

    for (k = 0; k < components_number; ++k) {
        free(components[k].mb_input_data);
        free(components[k].output_data);
    }
    free(new_buffer);
}

void parse_SOS() {
    uint16_t length;
    uint8_t scan_components;
    uint8_t Ss, Se, Ah, Al;

    int i;

    length = read_bits_16bit(&reader, 16);
    LOG_STDOUT("Length = %d.\n", length);

    scan_components = read_bits_8bit(&reader, 8);
    assert(scan_components == components_number);

    for (i = 0; i < scan_components; ++i) {
        uint8_t component_selector = read_bits_8bit(&reader, 8);
        uint8_t table_id_DC = read_bits_8bit(&reader, 4);
        uint8_t table_id_AC = read_bits_8bit(&reader, 4);
        assert(component_selector == components[i].id);
        assert(table_id_DC == 0 || table_id_DC == 1);
        assert(table_id_AC == 0 || table_id_AC == 1);
        components[i].table_id_AC = table_id_AC;
        components[i].table_id_DC = table_id_DC;
        LOG_STDOUT("Processed %d-th component: selector = %d, DC_table_id = %d, AC_table_id = %d.\n",
                   i + 1, component_selector, table_id_DC, table_id_AC);
    }

    Ss = read_bits_8bit(&reader, 8);
    Se = read_bits_8bit(&reader, 8);
    Ah = read_bits_8bit(&reader, 4);
    Al = read_bits_8bit(&reader, 4);
    assert(Ss == 0 && Se == 63 && Ah == 0 && Al == 0);

    decode_scan_data();
}

int parse_segment() {
    uint16_t marker = read_bits_16bit(&reader, 16);
    int ret = 0;

    LOG_STDOUT("%X: ", marker);

    if (marker == SOI) {
        LOG_STDOUT("Start Of Image (SOI) was read.\n");
    } else if (marker == COM) {
        uint16_t length = read_bits_16bit(&reader, 16);
        skip_bits(&reader, (length - 2) << 3);
        LOG_STDOUT("Comment (COM) was read and %d bytes were skipped.\n", length - 2);
    } else if (marker == DQT) {
        LOG_STDOUT("Define Quantization Tables (DQT) was read.\n");
        parse_DQT();
    } else if (marker == SOF0) {
        LOG_STDOUT("Start Of Frame, baseline DCT (SOF0) was read.\n");
        parse_SOF0();
    } else if (marker == DHT) {
        LOG_STDOUT("Define Huffman Tables (DHT) was read.\n");
        parse_DHT();
    } else if (marker == SOS) {
        LOG_STDOUT("Start Of Scan (SOS) was read.\n");
        parse_SOS();
    } else if (marker == EOI) {
        LOG_STDOUT("End Of Image (EOI) was read.\n");
        ret = 1;
    } else if ((marker & APPn_MASK) == APPn_MASK) {
        uint16_t length = read_bits_16bit(&reader, 16);
        skip_bits(&reader, (length - 2) << 3);
        LOG_STDOUT("Application-specific (App%X) was read and %d bytes were skipped.\n",
                   marker & ~APPn_MASK, length - 2);
    } else {
        PROCESS_ERROR("Unsupported marker.\n");
    }
    LOG_STDOUT("\n");

    goto end;

    fail:
    ret = -1;

    end:
    return ret;
}

void decode_JPEG() {
    int i;
    int ret = 0;

    quant_matrices = (uint8_t **) malloc(4 * sizeof(uint8_t *));
    for (i = 0; i < 4; ++i) {
        quant_matrices[i] = (uint8_t *) malloc(64 * sizeof(uint8_t));
    }
    huffman_trees_AC = (Huffman_node *) malloc(2 * sizeof(Huffman_node));
    huffman_trees_DC = (Huffman_node *) malloc(2 * sizeof(Huffman_node));
    for (i = 0; i < 2; ++i) {
        init_huffman_node(&huffman_trees_AC[i], 0);
        init_huffman_node(&huffman_trees_DC[i], 0);
    }

    while (!ret) {
        ret = parse_segment();
    }

    for (i = 0; i < 4; ++i) {
        free(quant_matrices[i]);
    }

    free(quant_matrices);
    free(components);
}

#define MAX_VALUE UINT8_MAX

int write_header(FILE *file, char *file_name, char *file_type, int width, int height) {
    if (fprintf(file, "%s\n%d %d\n%d\n", file_type, width, height, MAX_VALUE) < 0) {
        fprintf(stderr, "Couldn't write the output file header to the \"%s\".\n", file_name);
        return -1;
    }
    return 0;
}

int write_data(FILE *file, char *file_name, const uint8_t *data, int data_size) {
    if (fwrite(data, sizeof(uint8_t), data_size, file) != data_size) {
        fprintf(stderr, "Couldn't write the output file data to the file \"%s\".\n", file_name);
        return -1;
    }
    return 0;
}

int write_output_file(char *file_name, const uint8_t *output_data, int width, int height, int channels) {
    FILE *output_file = NULL;
    int data_size;
    char file_type[3] = "P5";
    int ret = 0;

    output_file = fopen(file_name, "wb");
    if (!output_file) {
        fprintf(stderr, "Couldn't open the output file \"%s\".\n", file_name);
        goto fail;
    }

    if (channels == 3) {
        file_type[1] = '6';
    }
    if (write_header(output_file, file_name, file_type, width, height) < 0) {
        goto fail;
    }

    data_size = width * height * channels;
    if (write_data(output_file, file_name, output_data, data_size) < 0) {
        goto fail;
    }

    goto end;

    fail:
    ret = -1;

    end:
    if (output_file && fclose(output_file) != 0) {
        fprintf(stderr, "Couldn't close the output file \"%s\".\n", file_name);
        ret = -1;
    }
    return ret;
}

int parse_args(int argc, char **argv, char **input_file_name, char **output_file_name) {
    if (argc != 3) {
        PROCESS_ERROR("Incorrect number of arguments.\n");
    }
    *input_file_name = argv[1];
    *output_file_name = argv[2];

    goto end;

    fail:
    return -1;

    end:
    return 0;
}

int main(int argc, char **argv) {
    char *input_file_name;
    char *output_file_name;
    size_t input_data_size;
    uint8_t *input_data;
    FILE *input_file = NULL;

    int ret = 0;

    if (parse_args(argc, argv, &input_file_name, &output_file_name) < 0) {
        goto fail;
    }

    input_file = fopen(input_file_name, "rb");
    if (!input_file) {
        PROCESS_ERROR("Couldn't open the input file \"%s\".\n", input_file_name);
    }

    fseek(input_file, 0, SEEK_END);
    input_data_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    input_data = (uint8_t *) malloc(input_data_size * sizeof(uint8_t));
    if (!input_data) {
        PROCESS_ERROR("Couldn't allocate memory for the input file data.\n");
    }

    if (fread(input_data, 1, input_data_size, input_file) != input_data_size) {
        PROCESS_ERROR("Couldn't read input file data.\n");
    }

    init_bitstream_reader(&reader, input_data, input_data_size << 3);

    decode_JPEG();

    if (write_output_file(output_file_name, output_data, frame_width, frame_height, components_number) < 0) {
        goto fail;
    }

    goto end;

    fail:
    ret = 1;

    end:
    free(input_data);
    free(output_data);

    if (input_file && fclose(input_file) != 0) {
        fprintf(stderr, "Couldn't close the input file \"%s\".\n", input_file_name);
        ret = 1;
    }
    return ret;
}
