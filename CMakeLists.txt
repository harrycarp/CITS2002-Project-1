cmake_minimum_required(VERSION 3.10)
project(Project_1 C)

set(CMAKE_C_STANDARD 99)

add_executable(PipeSim_Standalone pipesim.c)

add_executable(Project_1 main.c pipesim.c)