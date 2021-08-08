#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define MAX_VALUE UINT8_MAX

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
    uint8_t *input_data;
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

void fill_output_data(const uint8_t *input_data, int width, int height, uint8_t *output_data,
                      const uint16_t *resulting_partition, int clusters_number) {
    int i, j;
    double d = (double) MAX_VALUE / (clusters_number - 1);
    for (i = 0; i < width * height; ++i) {
        for (j = 0; j < clusters_number; ++j) {
            uint16_t l = resulting_partition[j];
            uint16_t r = resulting_partition[j + 1];
            if ((input_data[i] >= l) && (input_data[i] < r)) {
                output_data[i] = (uint8_t) (j * d);
                break;
            }
        }
    }
}

uint32_t *calc_histogram(uint32_t *histogram_values, const uint8_t *data, int w, int h) {
    int i;
    int draw_plot = 0;
    FILE *histogram_gnuplot;

    for (i = 0; i < w * h; ++i) {
        ++histogram_values[(int) data[i]];
    }

    /* Save values to draw a histogram plot using gnuplot utility */
    if (draw_plot) {
        histogram_gnuplot = fopen("histogram_gnuplot.txt", "w");
        for (i = 0; i < MAX_VALUE + 1; ++i) {
            fprintf(histogram_gnuplot, "%d %d\n", i, (int) histogram_values[i]);
        }
        fclose(histogram_gnuplot);
    }

    return histogram_values;
}

void calc_p(double *p, const uint32_t *histogram, int N) {
    int i;
    for (i = 0; i < MAX_VALUE + 1; ++i) {
        p[i] = (double) histogram[i] / N;
    }
}

void calc_p_sums(double *p_sums, const double *p) {
    int i;
    p_sums[0] = p[0];
    for (i = 1; i < MAX_VALUE + 1; ++i) {
        p_sums[i] = p[i] + p_sums[i - 1];
    }
}

void calc_mean_values(double *mean_values, const double *p) {
    int i;
    mean_values[0] = 0.;
    for (i = 1; i < MAX_VALUE + 1; ++i) {
        mean_values[i] = i * p[i] + mean_values[i - 1];
    }
}

double calc_segment_sigma(int l, int r, const double *p_sums, const double *mean_values) {
    double q, mean;
    double sigma = 0.;
    if (l == 0) {
        q = p_sums[r - 1];
        mean = mean_values[r - 1];
    } else {
        q = p_sums[r - 1] - p_sums[l - 1];
        mean = mean_values[r - 1] - mean_values[l - 1];
    }
    if (q != 0) {
        sigma = mean * mean / q;
    }
    return sigma;
}

void calc_partition(int current_cluster, uint16_t *current_partition, uint16_t *resulting_partition, double *max_sigma,
                    int clusters_number, const double *p_sums, const double *mean_values) {
    int i;
    if (current_cluster == clusters_number - 1) {
        double sigma = 0.;
        for (i = 0; i < clusters_number; ++i) {
            sigma += calc_segment_sigma(current_partition[i], current_partition[i + 1], p_sums, mean_values);
        }
        if (sigma > *max_sigma) {
            *max_sigma = sigma;
            memcpy(resulting_partition, current_partition, (clusters_number + 1) * sizeof(uint16_t));
        }
    } else {
        for (i = current_partition[current_cluster] + 1; i <= MAX_VALUE - clusters_number + current_cluster + 2; ++i) {
            /* Recursively generate the rest of the partition */
            current_partition[current_cluster + 1] = i;
            calc_partition(current_cluster + 1, current_partition, resulting_partition,
                           max_sigma, clusters_number, p_sums, mean_values);
        }
    }
}

int method_otsu(const uint8_t *input_data, int w, int h,
                int clusters_number, uint16_t *resulting_partition) {
    double max_sigma = 0.;

    uint32_t *histogram = (uint32_t *) calloc(MAX_VALUE + 1, sizeof(uint32_t));
    double *p = malloc((MAX_VALUE + 1) * sizeof(double));
    double *p_sums = malloc((MAX_VALUE + 1) * sizeof(double));
    double *mean_values = malloc((MAX_VALUE + 1) * sizeof(double));
    uint16_t *current_partition = malloc((clusters_number + 1) * sizeof(uint16_t));

    int i;
    int ret = 0;

    if (!histogram || !p || !p_sums || !mean_values || !current_partition) {
        fprintf(stderr, "Couldn't allocate memory for the internal data.\n");
        goto fail;
    }

    current_partition[0] = 0;
    current_partition[clusters_number] = 256;

    calc_histogram(histogram, input_data, w, h);
    calc_p(p, histogram, w * h);
    calc_p_sums(p_sums, p);
    calc_mean_values(mean_values, p);
    calc_partition(0, current_partition, resulting_partition, &max_sigma, clusters_number, p_sums, mean_values);

    goto end;

    fail:
    ret = -1;

    end:
    free(histogram);
    free(p);
    free(p_sums);
    free(mean_values);
    free(current_partition);
    return ret;
}

typedef struct options {
    char *input_file_name;
    char *output_file_name;
    int clusters_number;
} options;

int parse_args(int argc, char **argv, options *opts) {
    int ret = 0;

    if (argc != 4) {
        fprintf(stderr, "Incorrect number of arguments.\n");
        goto fail;
    }

    opts->input_file_name = argv[1];
    opts->output_file_name = argv[2];

    opts->clusters_number = atoi(argv[3]);
    if (opts->clusters_number <= 0) {
        fprintf(stderr, "Incorrect <clusters_number> value %s. Must be more than zero.\n", argv[3]);
        goto fail;
    }

    goto end;

    fail:
    ret = -1;

    end:
    return ret;
}

int main(int argc, char **argv) {
    uint8_t *input_data = NULL;
    uint8_t *output_data = NULL;
    uint16_t *resulting_partition = NULL;
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
    resulting_partition = malloc((opts.clusters_number + 1) * sizeof(uint16_t));
    if (!resulting_partition) {
        fprintf(stderr, "Couldn't allocate memory for the resulting clusters partition.\n");
        goto fail;
    }

    if (method_otsu(input_data, width, height, opts.clusters_number, resulting_partition) < 0) {
        goto fail;
    }

    fill_output_data(input_data, width, height, output_data, resulting_partition, opts.clusters_number);
    if (write_output_file(opts.output_file_name, output_data, width, height) < 0) {
        goto fail;
    }

    goto end;

    fail:
    ret = 1;

    end:
    free(input_data);
    free(output_data);
    free(resulting_partition);
    return ret;
}
