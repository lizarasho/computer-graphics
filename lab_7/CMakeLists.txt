cmake_minimum_required(VERSION 3.19)
project(lab7 C)

set(CMAKE_C_STANDARD 90)

add_executable(lab7 main.c crc.h zlib_decoder.h common.h bitstream_reader.h)