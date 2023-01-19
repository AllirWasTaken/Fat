cmake_minimum_required(VERSION 3.23)
project(Fat C)

set(CMAKE_C_STANDARD 99)

add_executable(Fat main.c FatStructures.h file_reader.c file_reader.h Fat12Table.c Fat12Table.h)
