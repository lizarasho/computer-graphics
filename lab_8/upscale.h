#ifndef LAB8_UPSCALE_H
#define LAB8_UPSCALE_H

#include "common.h"

#define SIGN(x) (((x) > 0) - ((x) < 0))

void get_neighbours(double x, int32_t side_length, double *left_neighbour, double *right_neighbour) {
    double max, min;
    double l, m;

    min = x - 1;
    max = x + 1;

    l = trunc(min + SIGN(min));

    if ((side_length & 1) == 0) {
        l -= 0.5;
    }

    m = l;
    while (min > m | m > max) {
        ++m;
    }
    *left_neighbour = m++;

    while (min > m | m > max) {
        ++m;
    }
    *right_neighbour = m;
}

double calc_rate(double d) {
    double rate;
    if (fabs(d) >= 1) {
        rate = 0;
    } else {
        rate = 1 - SIGN(d) * d;
    }
    return rate;
}

double linear_interpolation(double left_pixel, double right_pixel,
                            double left_pos, double right_pos,
                            double point) {
    double left_rate = calc_rate(left_pos - point);
    double right_rate = calc_rate(right_pos - point);
    return (left_rate * left_pixel + right_rate * right_pixel) / (left_rate + right_rate);
}

double pixel_to_position(int32_t pixel, int32_t side_length) {
    double center = (double) side_length / 2;
    double pos = pixel - center;
    if ((side_length & 1) == 0) {
        pos += 0.5;
    }
    return pos;
}

int32_t position_to_pixel(double x, int32_t side_length) {
    int32_t pixel = (int32_t) trunc(x);
    if (x < 0 && x != pixel) {
        --pixel;
    }
    pixel += (side_length - (side_length & 1)) / 2;
    return CLIP(0, pixel, side_length - 1);
}

uint8_t bilinear_interpolation(int32_t w_pos, int32_t h_pos,
                               const uint8_t *input, uint16_t in_width, uint16_t in_height,
                               uint16_t out_width, uint16_t out_height) {
#define AT(w, h) (input[(h) * in_width + (w)] / 255.0)

    double left_pos, right_pos;
    double down_pos, up_pos;
    int32_t left_pixel, right_pixel;
    int32_t down_pixel, up_pixel;
    double left_down_pixel, right_down_pixel;
    double left_up_pixel, right_up_pixel;
    double down_result, up_result;
    double result;

    double w_pos_in = pixel_to_position(w_pos, out_width) * in_width / out_width;
    double h_pos_in = pixel_to_position(h_pos, out_height) * in_height / out_height;

    get_neighbours(w_pos_in, in_width, &left_pos, &right_pos);
    get_neighbours(h_pos_in, in_height, &down_pos, &up_pos);

    down_pixel = position_to_pixel(down_pos, in_height);
    left_pixel = position_to_pixel(left_pos, in_width);
    up_pixel = position_to_pixel(up_pos, in_height);
    right_pixel = position_to_pixel(right_pos, in_width);

    left_down_pixel = AT(left_pixel, down_pixel);
    right_down_pixel = AT(right_pixel, down_pixel);
    left_up_pixel = AT(left_pixel, up_pixel);
    right_up_pixel = AT(right_pixel, up_pixel);

    down_result = linear_interpolation(left_down_pixel, right_down_pixel, left_pos, right_pos, w_pos_in);
    up_result = linear_interpolation(left_up_pixel, right_up_pixel, left_pos, right_pos, w_pos_in);

    result = linear_interpolation(down_result, up_result, down_pos, up_pos, h_pos_in);

    return (uint8_t) (CLIP(0, round(result * 255), 255));
}

void upscale(const uint8_t *input, uint16_t in_width, uint16_t in_height,
             uint8_t *output, uint16_t out_width, uint16_t out_height) {
    int i, j;
    for (i = 0; i < out_height; ++i) {
        for (j = 0; j < out_width; ++j) {
            output[i * out_width + j] = bilinear_interpolation(j, i, input, in_width, in_height, out_width, out_height);
        }
    }
}

#endif
