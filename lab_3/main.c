#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#define MAX_VALUE UINT8_MAX
#define BITS 8

int read_header(FILE *file, char *file_name, int *width, int *height) {
    char file_type[3];
    int max_value;
    int c;
    if (fscanf(file, "%s", file_type) != 1) {
        fprintf(stderr, "Incorrect input file \"%s\" format. Couldn't initialize file type.\n", file_name);
        return -1;
    }
    if (strcmp(file_type, "P5") != 0) {
        fprintf(stderr, "Incorrect type %s of the input file \"%s\". Only P5 type is supported.\n",
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
    if (fscanf(file, "%d", &max_value) != 1) {
        fprintf(stderr, "Incorrect input file \"%s\" format. Couldn't initialize the maximum color value.\n",
                file_name);
        return -1;
    }
    if (max_value != MAX_VALUE) {
        fprintf(stderr, "Incorrect maximum color value %d of the input file \"%s\". "
                        "Must be equals 255.\n", max_value, file_name);
        return -1;
    }
    c = fgetc(file);
    if (!isspace(c)) {
        fprintf(stderr, "Incorrect input file \"%s\" format: expected whitespace symbol, found \"%c\"", file_name, c);
        return -1;
    }
    return 0;
}

int write_header(FILE *file, char *file_name, char *file_type, int width, int height) {
    if (fprintf(file, "%s\n%d %d\n%d\n", file_type, width, height, MAX_VALUE) < 0) {
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

int write_data(FILE *file, char *file_name, const uint8_t *data, size_t data_size) {
    if (fwrite(data, sizeof(uint8_t), data_size, file) != data_size) {
        fprintf(stderr, "Couldn't write the output file data to the file \"%s\".\n", file_name);
        return -1;
    }
    return 0;
}

uint8_t *parse_input_file(char *file_name, int *width, int *height) {
    FILE *input_file = NULL;
    uint8_t *input_data = NULL;
    size_t data_size;
    int ret = 0;

    input_file = fopen(file_name, "rb");
    if (!input_file) {
        fprintf(stderr, "Couldn't open the input file \"%s\".\n", file_name);
        goto fail;
    }
    if (read_header(input_file, file_name, width, height) < 0) {
        goto fail;
    }
    data_size = *width * *height * sizeof(uint8_t);
    input_data = (uint8_t *) malloc(data_size);
    if (!input_data) {
        fprintf(stderr, "Couldn't allocate memory for the input file data.\n");
        goto fail;
    }
    if (read_data(input_file, file_name, input_data, data_size) < 0) {
        goto fail;
    }

    goto end;

    fail:
    ret = -1;

    end:
    if (input_file && fclose(input_file) != 0) {
        fprintf(stderr, "Couldn't close the input file \"%s\".\n", file_name);
        ret = -1;
    }
    if (ret == -1) {
        free(input_data);
        return NULL;
    }
    return input_data;
}

int write_output_file(char *file_name, const uint8_t *output_data, int width, int height) {
    FILE *output_file = NULL;
    size_t data_size;
    char file_type[3] = "P5";
    int ret = 0;

    output_file = fopen(file_name, "wb");
    if (!output_file) {
        fprintf(stderr, "Couldn't open the output file \"%s\".\n", file_name);
        goto fail;
    }
    if (write_header(output_file, file_name, file_type, width, height) < 0) {
        goto fail;
    }

    data_size = width * height * sizeof(uint8_t);
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

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))
#define CLIP(val) (MIN(MAX(0, val), MAX_VALUE))

void generate_gradient(double *buffer, int w, int h) {
    double delta = (double) MAX_VALUE / (MAX(1, w - 1));
    int i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            buffer[i * w + j] = delta * j;
        }
    }
}

uint8_t cut_bits(uint8_t in, int bits_limit) {
    uint8_t out = 0;
    int i;
    int remain = BITS % bits_limit;
    int j = remain ? remain - 1 : bits_limit - 1;
    for (i = 0; i < BITS; ++i) {
        uint8_t k = (in >> (BITS - 1 - j)) & 1;
        out |= (k << i);
        if (j == 0) {
            j = bits_limit;
        }
        j--;
    }
    return out;
}

void get_neighboring_values(uint8_t value, uint8_t *left, uint8_t *right, int bits_limit) {
    int delta = 1 << (8 - bits_limit);
    *left = cut_bits(value, bits_limit);
    *right = cut_bits(CLIP(value + delta), bits_limit);
    if (*left == *right) {
        *left = cut_bits(CLIP(*right - delta), bits_limit);
    } else if (*left > value) {
        *right = *left;
        *left = cut_bits(CLIP(*left - delta), bits_limit);
    }
}

double sRGB(double u) {
    if (u <= 0.04045) {
        return u / 12.92;
    }
    return pow((u + 0.055) / 1.055, 2.4);
}

double convert_gamma(double u, double gamma) {
    if (gamma != 0) {
        return pow(u, gamma);
    }
    return sRGB(u);
}

void no_dithering(double *buffer, uint8_t *output_data, int w, int h, int bits_limit) {
    int i;
    for (i = 0; i < w * h; ++i) {
        output_data[i] = cut_bits(round(buffer[i]), bits_limit);
    }
}

#define CALC_GAMMAS() \
    v = buffer[i * w + j];                                          \
    get_neighboring_values((uint8_t) trunc(v), &l, &r, bits_limit); \
    gamma_l = convert_gamma((double) l / MAX_VALUE, gamma);         \
    gamma_v = convert_gamma(v / MAX_VALUE, gamma);                  \
    gamma_r = convert_gamma((double) r / MAX_VALUE, gamma);

void basic_dithering(const double *buffer, uint8_t *output_data, int w, int h, int bits_limit,
                     double gamma, double (*calc_error)(int, int)) {
    int i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            uint8_t l, r;
            double v;
            double gamma_l, gamma_v, gamma_r;
            double errored_gamma_v;
            double error;

            CALC_GAMMAS()

            error = (*calc_error)(i, j);

            errored_gamma_v = gamma_v + (gamma_r - gamma_l) * error;
            if (errored_gamma_v < (gamma_l + gamma_r) / 2) {
                output_data[i * w + j] = l;
            } else {
                output_data[i * w + j] = r;
            }
        }
    }
}

#define DISSIPATION_W 5
#define DISSIPATION_H 3

typedef struct error_dissipation_matrix {
    double matrix[DISSIPATION_H][DISSIPATION_W];
    int common_divisor;
} error_dissipation_matrix;


void error_dissipating_dithering(const double *buffer, uint8_t *output_data, int w, int h, int bits_limit,
                                 double gamma, const error_dissipation_matrix *errors_matrix) {
    int i, j;

    double *errors_accum = (double *) calloc(w * h, sizeof(double));

    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            int y, x;
            uint8_t l, r;
            double v;
            double gamma_l, gamma_v, gamma_r;
            double errored_gamma_v;
            double error;
            double total_neighboring_error;

            CALC_GAMMAS()

            error = errors_accum[i * w + j];
            errored_gamma_v = gamma_v + error;
            if (errored_gamma_v < gamma_r) {
                output_data[i * w + j] = l;
                total_neighboring_error = errored_gamma_v - gamma_l;
            } else {
                output_data[i * w + j] = r;
                total_neighboring_error = errored_gamma_v - gamma_r;
            }
            /* Dissipate total_neighboring_error for the neighboring values */
            for (x = 0; x < DISSIPATION_H; ++x) {
                for (y = 0; y < DISSIPATION_W; ++y) {
                    if ((j + y - 2 >= 0) && (j + y - 2 < w) && (i + x >= 0) && (i + x < h)) {
                        double weight = errors_matrix->matrix[x][y] / errors_matrix->common_divisor;
                        errors_accum[(i + x) * w + (j + y - 2)] += total_neighboring_error * weight;
                    }
                }
            }
        }
    }
}

double calc_ordered_error(int i, int j) {
    int error_matrix[8][8] = {
            {0,  32, 8,  40, 2,  34, 10, 42},
            {48, 16, 56, 24, 50, 18, 58, 26},
            {12, 44, 4,  36, 14, 46, 6,  38},
            {60, 28, 52, 20, 62, 30, 54, 22},
            {3,  35, 11, 43, 1,  33, 9,  41},
            {51, 19, 59, 27, 49, 17, 57, 25},
            {15, 47, 7,  39, 13, 45, 5,  37},
            {63, 31, 55, 23, 61, 29, 53, 21}
    };
//    return (double) (error_matrix[i % 8][j % 8]) / 64;
    return (double) (error_matrix[i % 8][j % 8] + 0.5) / 64 - 0.5;
}

double calc_random_error(int i, int j) {
    return (double) rand() / (RAND_MAX + 1) - 0.5;
}

double calc_halftone_error(int i, int j) {
    int error_matrix[4][4] = {
            {7,  13, 11, 4},
            {12, 16, 14, 8},
            {10, 15, 6,  2},
            {5,  9,  3,  1}
    };
    return (double) error_matrix[i % 4][j % 4] / 17 - 0.5;
}

/* Floyd–Steinberg matrix */
static const error_dissipation_matrix FS_MATRIX = {
        {
                {0, 0, 0, 7, 0},
                {0, 3, 5, 1, 0},
                {0, 0, 0, 0, 0}
        },
        16
};

/* Jarvis, Judice, Ninke matrix */
static const error_dissipation_matrix JJN_MATRIX = {
        {
                {0, 0, 0, 7, 5},
                {3, 5, 7, 5, 3},
                {1, 3, 5, 3, 1}
        },
        48
};

static const error_dissipation_matrix SIERRA_MATRIX = {
        {
                {0, 0, 0, 5, 3},
                {2, 4, 5, 4, 2},
                {0, 2, 3, 2, 0}
        },
        32
};

static const error_dissipation_matrix ATKINSON_MATRIX = {
        {
                {0, 0, 0, 1, 1},
                {0, 1, 1, 1, 0},
                {0, 0, 1, 0, 0}
        },
        8
};

void convert_data(const uint8_t *input_data, uint8_t *output_data, int w, int h,
                  int gradient, int dithering, int bits_limit, double gamma) {
#define NO_DITHERING \
            no_dithering(buffer, output_data, w, h, bits_limit); \
            break;
#define BASIC_DITHERING(calc_error) \
            basic_dithering(buffer, output_data, w, h, bits_limit, gamma, &(calc_error));     \
            break
#define ERROR_DISSIPATING_DITHERING(errors_matrix) \
            error_dissipating_dithering(buffer, output_data, w, h, bits_limit, gamma, &(errors_matrix));    \
            break
    double *buffer = (double *) malloc(w * h * sizeof(double));
    srand(time(NULL));
    if (gradient == 1) {
        generate_gradient(buffer, w, h);
    } else {
        memcpy(output_data, input_data, w * h);
    }
    switch (dithering) {
        case 0:
        NO_DITHERING
        case 1:
        BASIC_DITHERING(calc_ordered_error);
        case 2:
        BASIC_DITHERING(calc_random_error);
        case 3:
        ERROR_DISSIPATING_DITHERING(FS_MATRIX);
        case 4:
        ERROR_DISSIPATING_DITHERING(JJN_MATRIX);
        case 5:
        ERROR_DISSIPATING_DITHERING(SIERRA_MATRIX);
        case 6:
        ERROR_DISSIPATING_DITHERING(ATKINSON_MATRIX);
        case 7:
        BASIC_DITHERING(calc_halftone_error);
    }
    free(buffer);
}

typedef struct options {
    char *input_file_name;
    char *output_file_name;
    int gradient;
    int dithering;
    int bits_limit;
    double gamma;
} options;

int parse_args(int argc, char **argv, options *opts) {
    int ret = 0;

    if (argc != 7) {
        fprintf(stderr, "Incorrect number of arguments.\n");
        goto fail;
    }

    opts->input_file_name = argv[1];
    opts->output_file_name = argv[2];

    opts->gradient = atoi(argv[3]);
    if (opts->gradient != 0 && opts->gradient != 1) {
        fprintf(stderr, "Incorrect <gradient> value. Must be equals 0 or 1.\n");
        goto fail;
    }

    opts->dithering = atoi(argv[4]);
    if (opts->dithering > 7 || opts->dithering < 0) {
        fprintf(stderr, "Incorrect <dithering> value. Must be in range [0; 7].\n");
        goto fail;
    }

    opts->bits_limit = atoi(argv[5]);
    if (opts->bits_limit > 8 || opts->bits_limit < 1) {
        fprintf(stderr, "Incorrect <bits_limit> value. Must be in range [1; 8].\n");

    }

    opts->gamma = atof(argv[6]);

    goto end;

    fail:
    ret = -1;

    end:
    return ret;
}

int main(int argc, char **argv) {
    uint8_t *input_data = NULL;
    uint8_t *output_data = NULL;
    int width, height;
    options opts;
    int ret = 0;

    if (parse_args(argc, argv, &opts) < 0) {
        goto fail;
    }

    input_data = parse_input_file(opts.input_file_name, &width, &height);
    if (!input_data) {
        goto fail;
    }

    output_data = (uint8_t *) malloc(width * height);
    if (!output_data) {
        fprintf(stderr, "Couldn't allocate memory for the output data.\n");
        goto fail;
    }

    convert_data(input_data, output_data, width, height, opts.gradient, opts.dithering, opts.bits_limit, opts.gamma);

    if (write_output_file(opts.output_file_name, output_data, width, height) < 0) {
        goto fail;
    }

    goto end;

    fail:
    ret = 1;

    end:
    free(input_data);
    free(output_data);

    return ret;
}