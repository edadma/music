cmake_minimum_required(VERSION 3.31)
project(music C)

set(CMAKE_C_STANDARD 11)

add_executable(music main.c music.c fast_math.c pulseaudio_driver.c)
target_link_libraries(music m pulse-simple pulse)
