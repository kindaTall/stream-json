cmake_minimum_required(VERSION 3.10)
project(stream_json C)

set(CMAKE_C_STANDARD 99)

# Library source files
set(LIB_SOURCES
    src/stream_json_write.c
)

set(LIB_HEADERS
    src/stream_json.h
)

# Create static library
add_library(stream_json STATIC ${LIB_SOURCES} ${LIB_HEADERS})
target_include_directories(stream_json PUBLIC src)

# Example executable
add_executable(write_examples examples/write_examples.c)
target_link_libraries(write_examples stream_json)

# Set output directories
set_target_properties(stream_json write_examples
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
