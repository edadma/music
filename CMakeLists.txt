cmake_minimum_required(VERSION 3.31)
project(musicbox C)

set(CMAKE_C_STANDARD 11)

# Find required packages
find_package(PkgConfig REQUIRED)

# Find PipeWire
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

# Add executable with all source files
add_executable(musicbox
        array.c
        instrument.c
        main.c
        parser.c
        pw_driver.c
        sequencer.c
        test.c
)

# Link libraries and set include directories
target_link_libraries(musicbox
        ${PIPEWIRE_LIBRARIES}
        m  # Math library for sin(), etc.
)

target_include_directories(musicbox PRIVATE
        ${PIPEWIRE_INCLUDE_DIRS}
)

target_compile_options(musicbox PRIVATE
        ${PIPEWIRE_CFLAGS_OTHER}
)

# Optional: Enable debug info and warnings
target_compile_options(musicbox PRIVATE -Wall -Wextra -g)

# Print configuration info (helpful for debugging)
message(STATUS "PipeWire found:")
message(STATUS "  Version: ${PIPEWIRE_VERSION}")
message(STATUS "  Libraries: ${PIPEWIRE_LIBRARIES}")
message(STATUS "  Include dirs: ${PIPEWIRE_INCLUDE_DIRS}")
message(STATUS "  Compile flags: ${PIPEWIRE_CFLAGS_OTHER}")