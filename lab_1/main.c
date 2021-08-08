#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

void inverse(const uint8_t *input, int channels, int w, int h, uint8_t *output) {
    int i;
    for (i = 0; i < channels * w * h; ++i) {
        output[i] = UINT8_MAX - input[i];
    }
}

void reflect_hor(const uint8_t *input, int channels, int w, int h, uint8_t *output) {
    int i, j, c;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            for (c = 0; c < channels; ++c) {
                output[i * (channels * w) + channels * j + c] = input[i * (channels * w) + channels * (w - 1 - j) + c];
            }
        }
    }
}

void reflect_vert(const uint8_t *input, int channels, int w, int h, uint8_t *output) {
    int i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < channels * w; ++j) {
            output[i * (channels * w) + j] = input[(h - 1 - i) * (channels * w) + j];
        }
    }
}

void rotate_right(const uint8_t *input, int channels, int w, int h, uint8_t *output) {
    int i, j, c;
    for (i = 0; i < w; ++i) {
        for (j = 0; j < h; ++j) {
            for (c = 0; c < channels; ++c) {
                output[i * (channels * h) + channels * j + c] = input[(h - 1 - j) * (channels * w) + channels * i + c];
            }
        }
    }
}

void rotate_left(const uint8_t *input, int channels, int w, int h, uint8_t *output) {
    int i, j, c;
    for (i = 0; i < w; ++i) {
        for (j = 0; j < h; ++j) {
            for (c = 0; c < channels; ++c) {
                output[i * (channels * h) + channels * j + c] = input[j * (channels * w) + channels * (w - 1 - i) + c];
            }
        }
    }
}

int transform(int channels, int transform_type,
              uint8_t *input_data, int in_width, int in_height,
              uint8_t *output_data, int *out_width, int *out_height) {
    switch (transform_type) {
        case 0:
            inverse(input_data, channels, in_width, in_height, output_data);
            *out_width = in_width;
            *out_height = in_height;
            break;
        case 1:
            reflect_hor(input_data, channels, in_width, in_height, output_data);
            *out_width = in_width;
            *out_height = in_height;
            break;
        case 2:
            reflect_vert(input_data, channels, in_width, in_height, output_data);
            *out_width = in_width;
            *out_height = in_height;
            break;
        case 3:
            rotate_right(input_data, channels, in_width, in_height, output_data);
            *out_width = in_height;
            *out_height = in_width;
            break;
        case 4:
            rotate_left(input_data, channels, in_width, in_height, output_data);
            *out_width = in_height;
            *out_height = in_width;
            break;
        default:
            fprintf(stderr, "Incorrect transform type %d. Must be less than 5 and greater than or equal to 0.\n",
                    transform_type);
            return -1;
    }
    return 0;
}

int read_header(FILE *file, char *file_name, char *file_type, int *channels,
                int *width, int *height, int *max_value) {
    int c;
    if (fscanf(file, "%s", file_type) != 1) {
        fprintf(stderr, "Incorrect input file \"%s\" format. Couldn't initialize file type.\n", file_name);
        return -1;
    }
    if (!strcmp(file_type, "P5")) {
        *channels = 1;
    } else if (!strcmp(file_type, "P6")) {
        *channels = 3;
    } else {
        fprintf(stderr, "Incorrect type %s of the input file \"%s\". Only P5 and P6 types are supported.\n",
                file_type, file_name);
        return -1;
    }
    if (fscanf(file, "%d %d", width, height) != 2) {
        fprintf(stderr, "Incorrect input file \"%s\" format. Couldn't initialize width and height.\n", file_name);
        return -1;
    }
    if ((*width <= 0) || (*height <= 0)) {
        fprintf(stderr, "Incorrect width or height of the input file \"%s\". Must be more than 0.\n", file_name);
        return -1;
    }
    if (fscanf(file, "%d", max_value) != 1) {
        fprintf(stderr, "Incorrect input file \"%s\" format. Couldn't initialize the maximum color value.\n",
                file_name);
        return -1;
    }
    if ((*max_value < 0) || (*max_value > UINT8_MAX)) {
        fprintf(stderr, "Incorrect maximum color value %d of the input file \"%s\". "
                        "Must be less than 256 and more than 0.\n", *max_value, file_name);
        return -1;
    }
    c = fgetc(file);
    if (!isspace(c)) {
        fprintf(stderr, "Incorrect input file \"%s\" format: expected whitespace symbol, found \"%c\"", file_name, c);
        return -1;
    }
    return 0;
}

int write_header(FILE *file, char *file_name, char *file_type, int width, int height, int max_value) {
    if (fprintf(file, "%s\n%d %d\n%d\n", file_type, width, height, max_value) < 0) {
        fprintf(stderr, "Couldn't write the output file header to the \"%s\".\n", file_name);
        return -1;
    }
    return 0;
}

int read_data(FILE *file, char *file_name, uint8_t *data, size_t data_size) {
    if (fread(data, sizeof(uint8_t), data_size, file) != data_size) {
        fprintf(stderr, "Couldn't read the input file data from the file \"%s\".\n", file_name);
        return -1;
    }
    return 0;
}

int write_data(FILE *file, char *file_name, uint8_t *data, size_t data_size) {
    if (fwrite(data, sizeof(uint8_t), data_size, file) != data_size) {
        fprintf(stderr, "Couldn't write the output file data to the file \"%s\".\n", file_name);
        return -1;
    }
    return 0;
}

int main(int argv, char **argc) {
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    int transform_type;
    char file_type[3];
    int in_width, in_height, max_value;
    int out_width, out_height;
    int channels;
    uint8_t *input_data = NULL;
    uint8_t *output_data = NULL;
    size_t data_size;
    int ret = 0;

    if (argv != 4) {
        fprintf(stderr, "Incorrect arguments format.\n"
                        "Usage: %s <input_file_name> <output_file_name> <transform_type>\n", argc[0]);
        goto fail;
    }

    input_file = fopen(argc[1], "rb");
    if (!input_file) {
        fprintf(stderr, "Couldn't open the input file \"%s\".\n", argc[1]);
        goto fail;
    }

    if (read_header(input_file, argc[1], file_type, &channels, &in_width, &in_height, &max_value) < 0) {
        goto fail;
    }

    data_size = channels * in_width * in_height * sizeof(uint8_t);

    input_data = (uint8_t *) malloc(data_size);
    if (!input_data) {
        fprintf(stderr, "Couldn't allocate memory for the input file \"%s\"data.\n", argc[1]);
        goto fail;
    }

    output_data = (uint8_t *) malloc(data_size);
    if (!output_data) {
        fprintf(stderr, "Couldn't allocate memory for the output file \"%s\" data.\n", argc[2]);
        goto fail;
    }

    if (read_data(input_file, argc[1], input_data, data_size) < 0) {
        goto fail;
    }

    transform_type = atoi(argc[3]);
    if (transform(channels, transform_type, input_data, in_width, in_height,
                  output_data, &out_width, &out_height) < 0) {
        goto fail;
    }

    output_file = fopen(argc[2], "wb");
    if (!output_file) {
        fprintf(stderr, "Couldn't open the output file \"%s\".\n", argc[2]);
        goto fail;
    }

    if (write_header(output_file, argc[2], file_type, out_width, out_height, max_value) < 0) {
        goto fail;
    }
    if (write_data(output_file, argc[2], output_data, data_size) < 0) {
        goto fail;
    }

    goto end;

    fail:
    ret = 1;

    end:
    free(input_data);
    free(output_data);
    if (input_file) {
        if (fclose(input_file) != 0) {
            fprintf(stderr, "Couldn't close the input file \"%s\".\n", argc[1]);
            ret = 1;
        }
    }
    if (output_file) {
        if (fclose(output_file) != 0) {
            fprintf(stderr, "Couldn't close the output file \"%s\".\n", argc[2]);
            ret = 1;
        }
        if ((ret != 0) && !remove(argc[2])) {
            fprintf(stderr, "Couldn't remove the output file \"%s\" with partially written data.\n", argc[2]);
        }
    }
    return ret;
}
