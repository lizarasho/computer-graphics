#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_VALUE UINT8_MAX
#define D (double) UINT8_MAX / 2
#define CHANNELS 3

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))
#define CLIP(min, val, max) (MIN(MAX(min, val), max))

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
    if (*max_value != MAX_VALUE) {
        fprintf(stderr, "Incorrect maximum color value %d of the input file \"%s\". "
                        "Must be equals 255.\n", *max_value, file_name);
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

int write_data(FILE *file, char *file_name, uint8_t *data, size_t data_size) {
    if (fwrite(data, sizeof(uint8_t), data_size, file) != data_size) {
        fprintf(stderr, "Couldn't write the output file data to the file \"%s\".\n", file_name);
        return -1;
    }
    return 0;
}

void RGB_to_YCbCr(const uint8_t *input, int w, int h, uint8_t *output, double Kr, double Kb) {
    int i, j;
    double Kg = 1 - Kr - Kb;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            int d = i * (CHANNELS * w) + CHANNELS * j;
            double R = (double) input[d] / MAX_VALUE;
            double G = (double) input[d + 1] / MAX_VALUE;
            double B = (double) input[d + 2] / MAX_VALUE;
            double Y = (Kr * R + Kg * G + Kb * B);
            double Cb = (-0.5 * Kr / (1 - Kb) * R - 0.5 * Kg / (1 - Kb) * G + 0.5 * B) + 0.5; // -0.5..0.5 -> 0..1
            double Cr = (0.5 * R - 0.5 * Kg / (1 - Kr) * G - 0.5 * Kb / (1 - Kr) * B) + 0.5;  // -0.5..0.5 -> 0..1
            output[d] = CLIP(0, Y * MAX_VALUE, MAX_VALUE);
            output[d + 1] = CLIP(0, Cb * MAX_VALUE, MAX_VALUE);
            output[d + 2] = CLIP(0, Cr * MAX_VALUE, MAX_VALUE);
        }
    }
}

void YCbCr_to_RGB(const uint8_t *input, int w, int h, uint8_t *output, double Kr, double Kb) {
    int i, j;
    double Kg = 1 - Kr - Kb;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            int d = i * (CHANNELS * w) + CHANNELS * j;
            double Y = (double) input[d] / MAX_VALUE;               // 0..1
            double Cb = (double) input[d + 1] / MAX_VALUE - 0.5;    // 0..1 -> -0.5..0.5
            double Cr = (double) input[d + 2] / MAX_VALUE - 0.5;    // 0..1 -> -0.5..0.5
            double R = Y + Cr * (2. - 2. * Kr);
            double G = Y - Kb / Kg * (2. - 2. * Kb) * Cb - Kr / Kg * (2. - 2. * Kr) * Cr;
            double B = Y + (2. - 2. * Kb) * Cb;
            output[d] = CLIP(0, R * MAX_VALUE, MAX_VALUE);
            output[d + 1] = CLIP(0, G * MAX_VALUE, MAX_VALUE);
            output[d + 2] = CLIP(0, B * MAX_VALUE, MAX_VALUE);
        }
    }
}

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))
#define IS_ZERO(a) ((a)<1e-9)

void RGB_to_HSV(const uint8_t *input, int w, int h, uint8_t *output) {
    int i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            int d = i * (CHANNELS * w) + CHANNELS * j;
            double R = (double) input[d] / MAX_VALUE;
            double G = (double) input[d + 1] / MAX_VALUE;
            double B = (double) input[d + 2] / MAX_VALUE;
            double C_min = MIN(R, MIN(G, B));
            double C_max = MAX(R, MAX(G, B));
            double delta = C_max - C_min;
            double H, S, V;

            /* Calculate Hue, H \in 0..360 */
            if (IS_ZERO(fabs(delta))) {
                H = 0;
            } else if (C_max == R) {
                H = fmod((G - B) / delta, 6.0);
                if (H < 0.0) H += 6.0;
            } else if (C_max == G) {
                H = 2.0 + (B - R) / delta;
            } else { /* C_max == B */
                H = 4.0 + (R - G) / delta;
            }
            H *= 60.0;

            /* Calculate Saturation, S \in 0..1 */
            if (IS_ZERO(fabs(C_max))) {
                S = 0;
            } else {
                S = delta / C_max;
            }

            /* Calculate Value, V \in 0..1 */
            V = C_max;

            output[d] = H / 360.0 * MAX_VALUE;    // 0..360 -> 0..255
            output[d + 1] = S * MAX_VALUE;        // 0..1 -> 0..255
            output[d + 2] = V * MAX_VALUE;        // 0..1 -> 0..255
        }
    }
}

void HSVL_to_RGB(double H, double C, double *R, double *G, double *B) {
    double X = C * (1 - fabs(fmod(H / 60.0, 2.0) - 1));
    *R = 0;
    *G = 0;
    *B = 0;
    if (0 <= H && H < 60) {
        *R = C;
        *G = X;
    } else if (60 <= H && H < 120) {
        *R = X;
        *G = C;
    } else if (120 <= H && H < 180) {
        *G = C;
        *B = X;
    } else if (180 <= H && H < 240) {
        *G = X;
        *B = C;
    } else if (240 <= H && H < 300) {
        *R = X;
        *B = C;
    } else {
        *R = C;
        *B = X;
    }
}

void HSV_to_RGB(const uint8_t *input, int w, int h, uint8_t *output) {
    int i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            int d = i * (CHANNELS * w) + CHANNELS * j;
            double H = (double) input[d] * 360.0 / MAX_VALUE;   // 0..360
            double S = (double) input[d + 1] / MAX_VALUE;       // 0..1
            double V = (double) input[d + 2] / MAX_VALUE;       // 0..1
            double C = V * S;
            double m;
            double R, G, B;

            m = V - C;
            HSVL_to_RGB(H, C, &R, &G, &B);

            output[d] = (R + m) * MAX_VALUE;
            output[d + 1] = (G + m) * MAX_VALUE;
            output[d + 2] = (B + m) * MAX_VALUE;
        }
    }
}

void RGB_to_HSL(const uint8_t *input, int w, int h, uint8_t *output) {
    int i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            int d = i * (CHANNELS * w) + CHANNELS * j;
            double R = (double) input[d] / MAX_VALUE;
            double G = (double) input[d + 1] / MAX_VALUE;
            double B = (double) input[d + 2] / MAX_VALUE;
            double C_min = MIN(R, MIN(G, B));
            double C_max = MAX(R, MAX(G, B));
            double delta = C_max - C_min;
            double H, S, L;

            /* Calculate lightness, L \in 0..1 */
            L = (C_max + C_min) / 2.0;

            /* Calculate Hue, H \in 0..360 */
            if (IS_ZERO(fabs(delta))) {
                H = 0;
            } else if (C_max == R) {
                H = fmod((G - B) / delta, 6.0);
                if (H < 0.0) H += 6.0;
            } else if (C_max == G) {
                H = 2.0 + (B - R) / delta;
            } else { /* C_max == B */
                H = 4.0 + (R - G) / delta;
            }
            H *= 60.0;

            /* Calculate Saturation, S \in 0..1 */
            if (IS_ZERO(fabs(delta))) {
                S = 0;
            } else {
                S = delta / (1 - fabs(2 * L - 1));
            }

            output[d] = H / 360.0 * MAX_VALUE;    // 0..360 -> 0..255
            output[d + 1] = S * MAX_VALUE;        // 0..1 -> 0..255
            output[d + 2] = L * MAX_VALUE;        // 0..1 -> 0..255
        }
    }
}

void HSL_to_RGB(const uint8_t *input, int w, int h, uint8_t *output) {
    int i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            int d = i * (CHANNELS * w) + CHANNELS * j;
            double H = (double) input[d] * 360.0 / MAX_VALUE;   // 0..360
            double S = (double) input[d + 1] / MAX_VALUE;       // 0..1
            double L = (double) input[d + 2] / MAX_VALUE;       // 0..1
            double C = (1 - fabs(2 * L - 1)) * S;
            double m;
            double R, G, B;

            m = L - C / 2;
            HSVL_to_RGB(H, C, &R, &G, &B);

            output[d] = (R + m) * MAX_VALUE;
            output[d + 1] = (G + m) * MAX_VALUE;
            output[d + 2] = (B + m) * MAX_VALUE;
        }
    }
}

void RGB_to_YCbCr601(const uint8_t *input, int w, int h, uint8_t *output) {
    RGB_to_YCbCr(input, w, h, output, 0.299, 0.114);
}

void YCbCr601_to_RGB(const uint8_t *input, int w, int h, uint8_t *output) {
    YCbCr_to_RGB(input, w, h, output, 0.299, 0.114);
}

void RGB_to_YCbCr709(const uint8_t *input, int w, int h, uint8_t *output) {
    RGB_to_YCbCr(input, w, h, output, 0.2126, 0.0722);
}

void YCbCr709_to_RGB(const uint8_t *input, int w, int h, uint8_t *output) {
    YCbCr_to_RGB(input, w, h, output, 0.2126, 0.0722);
}

void RGB_to_YCoCg(const uint8_t *input, int w, int h, uint8_t *output) {
    int i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            int d = i * (CHANNELS * w) + CHANNELS * j;
            double R = (double) input[d] / MAX_VALUE;
            double G = (double) input[d + 1] / MAX_VALUE;
            double B = (double) input[d + 2] / MAX_VALUE;
            double Y = 0.25 * R + 0.5 * G + 0.25 * B;                       // 0..1
            double Co = 0.5 * R - 0.5 * B + 0.5;                            // -0.5..0.5 -> 0..1
            double Cg = -0.25 * R + 0.5 * G - 0.25 * B + 0.5;               // -0.5..0.5 -> 0..1
            output[d] = CLIP(0, Y * MAX_VALUE, MAX_VALUE);
            output[d + 1] = CLIP(0, Co * MAX_VALUE, MAX_VALUE);
            output[d + 2] = CLIP(0, Cg * MAX_VALUE, MAX_VALUE);
        }
    }
}

void YCoCg_to_RGB(const uint8_t *input, int w, int h, uint8_t *output) {
    int i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            int d = i * (CHANNELS * w) + CHANNELS * j;
            double Y = (double) input[d] / MAX_VALUE;               // 0..1
            double Co = (double) input[d + 1] / MAX_VALUE - 0.5;    // 0..1 -> -0.5..0.5
            double Cg = (double) input[d + 2] / MAX_VALUE - 0.5;    // 0..1 -> -0.5..0.5
            double R = Y + Co - Cg;
            double G = Y + Cg;
            double B = Y - Co - Cg;
            output[d] = CLIP(0, R * MAX_VALUE, MAX_VALUE);
            output[d + 1] = CLIP(0, G * MAX_VALUE, MAX_VALUE);
            output[d + 2] = CLIP(0, B * MAX_VALUE, MAX_VALUE);
        }
    }
}

void inverse(const uint8_t *input, int w, int h, uint8_t *output) {
    int i;
    for (i = 0; i < CHANNELS * w * h; ++i) {
        output[i] = UINT8_MAX - input[i];
    }
}

void RGB_to_CMY(const uint8_t *input, int w, int h, uint8_t *output) {
    inverse(input, w, h, output);
}

void CMY_to_RGB(const uint8_t *input, int w, int h, uint8_t *output) {
    inverse(input, w, h, output);
}

typedef enum space_name {
    RGB, HSV, HSL, YCbCr_601, YCbCr_709, YCoCg, CMY
} space_name;

typedef struct space_desc {
    space_name name;
    int files_number;
    char *file_name_template;
} space_desc;

typedef struct options {
    space_desc input_space;
    space_desc output_space;
} options;

#define OPT(param) else if (!strcmp(arg, param))

int parse_space_name(char *arg, space_name *name) {
    if (0);
    OPT("RGB") *name = RGB;
    OPT("HSL") *name = HSL;
    OPT("HSV") *name = HSV;
    OPT("YCbCr.601") *name = YCbCr_601;
    OPT("YCbCr.709") *name = YCbCr_709;
    OPT("YCoCg") *name = YCoCg;
    OPT("CMY") *name = CMY;
    else {
        fprintf(stderr, "Incorrect <color_space_name> = %s. Only RGB, HSL, HSV, "
                        "YCbCr.601, YCbCr.709, YCoCg and CMY color spaces are supported.\n", arg);
        return -1;
    }
    return 0;
}

int parse_files_number(char *files_number) {
    int result = atoi(files_number);
    if ((result != 1) && (result != 3)) {
        fprintf(stderr, "Incorrect <files_number> = %s. Must be equals 1 or 3.\n", files_number);
        return -1;
    }
    return result;
}


int parse_file_names(int *i, int argc, char **argv, space_desc *space) {
    int files_number = parse_files_number(argv[*i + 1]);
    if (files_number < 0) {
        return -1;
    }
    if (!strcmp("-f", argv[*i + 2]) || !strcmp("-t", argv[*i + 2]) ||
        !strcmp("-i", argv[*i + 2]) || !strcmp("-o", argv[*i + 2])) {
        fprintf(stderr, "Expected <file_name>, found option %s.\n", argv[*i + 2]);
        return -1;
    }
    space->files_number = files_number;
    space->file_name_template = argv[*i + 2];
    *i += 1;
    return 0;
}

int parse_args(int argc, char **argv, options *opts) {
    int i = 1;
    int ret = 0;

    if (argc != 11) {
        fprintf(stderr, "Incorrect number of arguments.\n");
        goto fail;
    }

    while (i < argc - 1) {
        char *arg = argv[i];
        if (0);
        OPT("-f") { if (parse_space_name(argv[i + 1], &opts->input_space.name) < 0) goto fail; }
        OPT("-t") { if (parse_space_name(argv[i + 1], &opts->output_space.name) < 0) goto fail; }
        OPT("-i") { if (parse_file_names(&i, argc, argv, &opts->input_space) < 0) goto fail; }
        OPT("-o") { if (parse_file_names(&i, argc, argv, &opts->output_space) < 0) goto fail; }
        else {
            fprintf(stderr, "Expected option, found %s.\n", arg);
            goto fail;
        }
        i += 2;
    }

    goto end;

    fail:
    ret = -1;

    end:
    return ret;
}

int generate_file_name(char *dest, char *template, char index) {
    size_t dot_index;
    int i;
    char *dot_ptr = strrchr(template, '.');
    if (!dot_ptr) {
        fprintf(stderr, "Incorrect <file_name_template> = %s. Must contain the extension of the file.", template);
        return -1;
    }
    dot_index = dot_ptr - template;
    for (i = 0; i < dot_index; ++i)
        dest[i] = template[i];
    dest[dot_index] = '_';
    dest[dot_index + 1] = index;
    for (i = dot_index; i < strlen(template); ++i)
        dest[i + 2] = template[i];
    dest[strlen(template) + 2] = '\0';
    return 0;
}

uint8_t *parse_input_files(int files_number, char *name_template, int *width, int *height) {
    int i;
    int ret = 0;
    uint8_t *input_data = NULL;

    for (i = 0; i < files_number; ++i) {
        FILE *input_file = NULL;
        uint8_t *data_buffer = NULL;
        int cur_width, cur_height;
        char file_name[strlen(name_template + 2)];
        char file_type[3];
        int channels;
        int max_value;
        size_t buffer_size;
        int j;

        if (files_number == 1) {
            strcpy(file_name, name_template);
        } else if (generate_file_name(file_name, name_template, '0' + i + 1) < 0) {
            goto loop_fail;
        }

        input_file = fopen(file_name, "rb");
        if (!input_file) {
            fprintf(stderr, "Couldn't open the input file \"%s\".\n", file_name);
            goto loop_fail;
        }
        if (read_header(input_file, file_name, file_type, &channels, &cur_width, &cur_height, &max_value) < 0) {
            goto loop_fail;
        }
        if ((files_number == 3) && (channels != 1) || (files_number == 1) && (channels != 3)) {
            fprintf(stderr, "Expected %s image in the input file \"%s\", found %s.\n",
                    files_number == 1 ? "P6" : "P5", file_name, files_number == 1 ? "P5" : "P6");
            goto loop_fail;
        }
        if (i == 0) {
            size_t data_size;
            *width = cur_width;
            *height = cur_height;
            data_size = CHANNELS * *width * *height * sizeof(uint8_t);
            input_data = (uint8_t *) malloc(data_size);
            if (!input_data) {
                fprintf(stderr, "Couldn't allocate memory for the input files data.\n");
                goto loop_fail;
            }
        } else {
            if ((*width != cur_width) || (*height != cur_height)) {
                fprintf(stderr, "Incorrect input files. All input images must have the same width and height.\n");
                goto loop_fail;
            }
        }
        buffer_size = channels * *width * *height * sizeof(uint8_t);
        data_buffer = (uint8_t *) malloc(buffer_size);
        if (!data_buffer) {
            fprintf(stderr, "Couldn't allocate memory for the input file \"%s\" data.\n", file_name);
            goto loop_fail;
        }
        if (read_data(input_file, file_name, data_buffer, buffer_size) < 0) {
            goto loop_fail;
        }
        for (j = 0; j < buffer_size; ++j) {
            if (files_number == 1) {
                input_data[j] = data_buffer[j];
            } else { /* files_number == 3 */
                input_data[j * CHANNELS + i] = data_buffer[j];
            }
        }
        goto loop_end;

        loop_fail:
        ret = -1;

        loop_end:
        free(data_buffer);
        if (input_file && fclose(input_file) != 0) {
            fprintf(stderr, "Couldn't close the input file \"%s\".\n", file_name);
            ret = -1;
        }
        if (ret == -1) { /* Error occurred during the for-loop */
            free(input_data);
            return NULL;
        }
    }

    return input_data;
}

int write_output_files(int files_number, char *name_template, const uint8_t *output_data, int width, int height) {
    int i;
    char file_name[strlen(name_template + 2)];
    int channels = files_number == 1 ? 3 : 1;
    char file_type[3] = "P_";
    int ret = 0;

    file_type[1] = files_number == 1 ? '6' : '5';

    for (i = 0; i < files_number; ++i) {
        FILE *output_file = NULL;
        uint8_t *data_buffer = NULL;
        size_t buffer_size;
        int j;

        if (files_number == 1) {
            strcpy(file_name, name_template);
        } else if (generate_file_name(file_name, name_template, '0' + i + 1) < 0) {
            goto loop_fail;
        }

        output_file = fopen(file_name, "wb");
        if (!output_file) {
            fprintf(stderr, "Couldn't open the output file \"%s\".\n", file_name);
            goto loop_fail;
        }
        if (write_header(output_file, file_name, file_type, width, height) < 0) {
            goto loop_fail;
        }

        buffer_size = channels * width * height * sizeof(uint8_t);
        data_buffer = (uint8_t *) malloc(buffer_size);
        if (!data_buffer) {
            fprintf(stderr, "Couldn't allocate memory for the output file \"%s\" data.\n", file_name);
            goto loop_fail;
        }
        for (j = 0; j < buffer_size; ++j) {
            if (files_number == 1) {
                data_buffer[j] = output_data[j];
            } else { /* files_number == 3 */
                data_buffer[j] = output_data[j * CHANNELS + i];
            }
        }
        if (write_data(output_file, file_name, data_buffer, buffer_size) < 0) {
            goto loop_fail;
        }

        goto loop_end;

        loop_fail:
        ret = -1;

        loop_end:
        free(data_buffer);
        if (output_file && fclose(output_file) != 0) {
            fprintf(stderr, "Couldn't close the output file \"%s\".\n", file_name);
            ret = -1;
        }
        if (ret == -1) { /* Error occurred during the for-loop */
            goto end;
        }
    }

    end:
    return ret;
}

int convert_data(space_name input_color_space, uint8_t *input_data, space_name output_color_space,
                 uint8_t *output_data, int width, int height) {
    int ret = 0;
    size_t data_size = CHANNELS * width * height;
    uint8_t *tmp_data = malloc(data_size);
    if (!tmp_data) {
        fprintf(stderr, "Couldn't allocate memory for the temporary data.\n");
        goto fail;
    }
    switch (input_color_space) {
        case RGB:
            memcpy(tmp_data, input_data, data_size);
            break;
        case HSV:
            HSV_to_RGB(input_data, width, height, tmp_data);
            break;
        case HSL:
            HSL_to_RGB(input_data, width, height, tmp_data);
            break;
        case YCbCr_601:
            YCbCr601_to_RGB(input_data, width, height, tmp_data);
            break;
        case YCbCr_709:
            YCbCr709_to_RGB(input_data, width, height, tmp_data);
            break;
        case YCoCg:
            YCoCg_to_RGB(input_data, width, height, tmp_data);
            break;
        case CMY:
            CMY_to_RGB(input_data, width, height, tmp_data);
            break;
        default:
            break;
    }
    switch (output_color_space) {
        case RGB:
            memcpy(output_data, tmp_data, data_size);
            break;
        case HSV:
            RGB_to_HSV(tmp_data, width, height, output_data);
            break;
        case HSL:
            RGB_to_HSL(tmp_data, width, height, output_data);
            break;
        case YCbCr_601:
            RGB_to_YCbCr601(tmp_data, width, height, output_data);
            break;
        case YCbCr_709:
            RGB_to_YCbCr709(tmp_data, width, height, output_data);
            break;
        case YCoCg:
            RGB_to_YCoCg(tmp_data, width, height, output_data);
            break;
        case CMY:
            RGB_to_CMY(tmp_data, width, height, output_data);
            break;
        default:
            break;
    }
    goto end;

    fail:
    ret = -1;

    end:
    free(tmp_data);
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

    input_data = parse_input_files(opts.input_space.files_number, opts.input_space.file_name_template, &width, &height);
    if (!input_data) {
        goto fail;
    }

    output_data = (uint8_t *) malloc(CHANNELS * width * height);
    if (!output_data) {
        fprintf(stderr, "Couldn't allocate memory for the output data.\n");
        goto fail;
    }

    if (convert_data(opts.input_space.name, input_data, opts.output_space.name, output_data, width, height) < 0) {
        goto fail;
    }
    if (write_output_files(opts.output_space.files_number, opts.output_space.file_name_template,
                           output_data, width, height) < 0) {
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
