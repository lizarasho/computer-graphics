cmake_minimum_required(VERSION 3.19)
project(lab8 C)

set(CMAKE_C_STANDARD 90)

add_executable(lab8 main.c common.h bitstream_reader.h huffman_decoder.h upscale.h)