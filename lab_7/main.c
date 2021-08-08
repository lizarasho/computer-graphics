#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "zlib_decoder.h"
#include "crc.h"

#define SIGNATURE_LENGTH 8
#define CHUNK_TYPE_LENGTH 4
#define CRC_LENGTH 4

uint8_t PNG_SIGNATURE[SIGNATURE_LENGTH] = {0x89, (uint8_t) 'P', (uint8_t) 'N', (uint8_t) 'G', 0x0D, 0x0A, 0x1A, 0x0A};

#define GET_CHUNK_TYPE(l1, l2, l3, l4) {(uint8_t) (l1), (uint8_t) (l2), (uint8_t) (l3), (uint8_t) (l4)}
uint8_t IHDR_TYPE[CHUNK_TYPE_LENGTH] = GET_CHUNK_TYPE('I', 'H', 'D', 'R');
uint8_t IDAT_TYPE[CHUNK_TYPE_LENGTH] = GET_CHUNK_TYPE('I', 'D', 'A', 'T');
uint8_t IEND_TYPE[CHUNK_TYPE_LENGTH] = GET_CHUNK_TYPE('I', 'E', 'N', 'D');

int parse_PNG_signature(FILE *file) {
    uint8_t signature_buffer[SIGNATURE_LENGTH];
    if (fread(signature_buffer, sizeof(uint8_t), SIGNATURE_LENGTH, file) != SIGNATURE_LENGTH
        || memcmp(signature_buffer, PNG_SIGNATURE, SIGNATURE_LENGTH) != 0) {
        PROCESS_ERROR("Couldn't read PNG signature: input file does not start with PNG signature.\n");
    }
    goto end;
    fail:
    return -1;
    end:
    return 0;
}

int process_IHDR_data(const uint8_t *data, int *width, int *height, int *channels) {
    uint8_t bit_depth;
    uint8_t compression_method;
    uint8_t filter_method;
    uint8_t interlace_method;
    uint8_t color_type;
    int ret = 0;

    LOG_STDOUT("Parsing IHDR data...\n");

    *width = BYTES_TO_INT(data);
    *height = BYTES_TO_INT(data + 4);
    if (*width == 0 || *height == 0) {
        PROCESS_ERROR("Invalid width or height value. Must be more than 0.\n");
    }

    bit_depth = data[8];
    if (bit_depth != 8) {
        PROCESS_ERROR("Unsupported bit depth %d. Must be equals to 8.\n", bit_depth);
    }
    color_type = data[9];
    if (color_type != 0 && color_type != 2) {
        PROCESS_ERROR("Unsupported color type %d. Must be equals to 0 or 2.\n", color_type);
    }
    *channels = color_type + 1;
    compression_method = data[10];
    if (compression_method != 0) {
        PROCESS_ERROR("Unsupported compression method %d. Must be equals to 0.\n", compression_method);
    }
    filter_method = data[11];
    if (filter_method != 0) {
        PROCESS_ERROR("Unsupported filter method %d. Must be equals to 0.\n", filter_method);
    }
    interlace_method = data[12];
    if (interlace_method != 0) {
        PROCESS_ERROR("Interlace method is not supported. Must be equals to 0.\n");
    }

    LOG_STDOUT("\tWidth: %d\n\tHeight: %d.\n\tColor type: %d\n", *width, *height, color_type);
    LOG_STDOUT("IHDR data was successfully parsed.\n");

    goto end;

    fail:
    ret = -1;

    end:
    return ret;
}

int process_IDAT_data(const uint8_t *chunk_data, uint32_t chunk_length, uint8_t *IDAT_data, size_t *IDAT_data_size) {
    memcpy(IDAT_data + *IDAT_data_size, chunk_data, chunk_length);
    *IDAT_data_size += chunk_length;
    LOG_STDOUT("IDAT chunk data was was saved for further decompressing.\n");
    return 0;
}

void process_IEND_data() {
    LOG_STDOUT("IEND data was successfully parsed.\n");
}

int is_ancillary(const uint8_t *chunk_type) {
    return chunk_type[0] >= (uint8_t) 'a' && chunk_type[0] <= (uint8_t) 'z';
}

int parse_chunk(FILE *file, int *width, int *height, int *channels, uint8_t *IDATs_data, size_t *IDATs_data_size) {
    uint8_t chunk_length_buffer[4];
    uint8_t crc_buffer[4];
    uint8_t chunk_type[CHUNK_TYPE_LENGTH];
    uint8_t *chunk_data = NULL;
    uint8_t *crc_data = NULL;
    uint32_t chunk_length;
    uint32_t actual_crc;
    uint32_t expected_crc;

    int ret = 0;

    LOG_STDOUT("Start chunk parsing.\n");

    // 1. parse Chunk Length
    if (fread(&chunk_length_buffer, sizeof(uint8_t), sizeof(uint32_t), file) != sizeof(uint32_t)) {
        PROCESS_ERROR("Couldn't read the chunk length.\n");
    }
    chunk_length = BYTES_TO_INT(chunk_length_buffer);
    LOG_STDOUT("Chunk length: %d.\n", chunk_length);

    chunk_data = malloc(chunk_length * sizeof(uint8_t));
    if (!chunk_data) {
        PROCESS_ERROR("Couldn't allocate memory for the chunk data.\n");
    }

    // 2. parse Chunk Type
    if (fread(chunk_type, sizeof(uint8_t), CHUNK_TYPE_LENGTH, file) != CHUNK_TYPE_LENGTH) {
        PROCESS_ERROR("Couldn't read the chunk type.\n");
    }

    // 3. parse Chunk Data
    if (fread(chunk_data, sizeof(uint8_t), chunk_length, file) != chunk_length) {
        PROCESS_ERROR("Couldn't read %d bytes of the chunk data.\n", chunk_length);
    }

    if (is_ancillary(chunk_type)) {
        // ignore ancillary chunk
        LOG_STDOUT("Chunk data was skipped: is ancillary.\n");
        ret = 1;
    } else {
        // process critical chunk
#define OPT(type) else if (!memcmp(chunk_type, type, CHUNK_TYPE_LENGTH))
        if (0);
        OPT(IHDR_TYPE) {
            ret = BYTES_TO_INT(IHDR_TYPE);
            if (process_IHDR_data(chunk_data, width, height, channels) < 0) {
                goto fail;
            }
        } OPT(IDAT_TYPE) {
            ret = BYTES_TO_INT(IDAT_TYPE);
            if (process_IDAT_data(chunk_data, chunk_length, IDATs_data, IDATs_data_size) < 0) {
                goto fail;
            }
        } OPT(IEND_TYPE) {
            ret = BYTES_TO_INT(IEND_TYPE);
            process_IEND_data();
        }
    }

    // 4. parse CRC
    if (fread(crc_buffer, sizeof(uint8_t), CRC_LENGTH, file) != CRC_LENGTH) {
        PROCESS_ERROR("Couldn't read the chunk length.\n");
    }
    crc_data = malloc(CHUNK_TYPE_LENGTH + chunk_length);
    memcpy(crc_data, chunk_type, CHUNK_TYPE_LENGTH);
    memcpy(crc_data + CHUNK_TYPE_LENGTH, chunk_data, chunk_length);
    expected_crc = crc(crc_data, CHUNK_TYPE_LENGTH + chunk_length);
    actual_crc = BYTES_TO_INT(crc_buffer);
    if (actual_crc != expected_crc) {
        PROCESS_ERROR("CRC value is invalid: the file has been transmitted damaged.\n");
    }

    goto end;

    fail:
    ret = -1;

    end:
    free(chunk_data);
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

int main(int argc, char **argv) {
    FILE *input_file = NULL;
    char *input_file_name;
    char *output_file_name;
    int width;
    int height;
    int channels;
    size_t max_IDATs_size;
    uint8_t *IDATs_data = NULL;
    size_t IDATs_data_ptr = 0;
    bitstream_reader reader;
    uint8_t *output_data;
    int current_chunk = 0;
    int ret = 0;

    if (parse_args(argc, argv, &input_file_name, &output_file_name) < 0) {
        goto fail;
    }

    input_file = fopen(input_file_name, "rb");
    if (!input_file) {
        PROCESS_ERROR("Couldn't open the input file \"%s\".\n", input_file_name);
    }

    if (parse_PNG_signature(input_file) < 0) {
        goto fail;
    }

    while (current_chunk != BYTES_TO_INT(IEND_TYPE)) {
        int parse_ret = parse_chunk(input_file, &width, &height, &channels, IDATs_data, &IDATs_data_ptr);
        if (parse_ret < 0) {
            goto fail;
        }
        if (current_chunk == 0) {
            max_IDATs_size = width * height * channels + height;
            if (parse_ret != BYTES_TO_INT(IHDR_TYPE)) {
                PROCESS_ERROR("IHDR chunk must be first.\n");
            }
            IDATs_data = (uint8_t *) malloc(max_IDATs_size);
        }
        current_chunk = parse_ret;
        LOG_STDOUT("\n");
    }

    output_data = (uint8_t *) malloc(width * height * channels);
    if (!output_data) {
        PROCESS_ERROR("Couldn't allocate memory for the output data.\n");
    }

    init_bitstream_reader(&reader, IDATs_data, max_IDATs_size << 3);
    if (parse_zlib_header(&reader) < 0) {
        goto fail;
    }
    decode_data(&reader, width, height, channels, output_data);

    if (write_output_file(output_file_name, output_data, width, height, channels) < 0) {
        goto fail;
    }

    if (fgetc(input_file) != EOF) {
        PROCESS_ERROR("IEND chunk must be last.\n");
    }

    LOG_STDOUT("Ok!\n");
    goto end;

    fail:
    ret = 1;

    end:
    free(IDATs_data);
    return ret;
}
