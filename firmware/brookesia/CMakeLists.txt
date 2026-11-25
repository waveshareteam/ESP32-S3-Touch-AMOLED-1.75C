cmake_minimum_required(VERSION 3.16)

# Reuse the board-tested Brookesia core and SquareLine demo from the ESP-IDF
# example.  The firmware keeps only its product-specific applications here so
# the large framework sources are not duplicated in the repository.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../examples/esp-idf/03_esp-brookesia/components/brookesia_core"
    "${CMAKE_CURRENT_LIST_DIR}/../../examples/esp-idf/03_esp-brookesia/components/brookesia_app_squareline_demo"
)

# Some reusable components live outside this project's CMAKE_SOURCE_DIR. Map
# the repository root as well so __FILE__, diagnostics, and debug metadata do
# not embed the build machine's checkout path in released firmware images.
get_filename_component(REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
file(TO_CMAKE_PATH "${REPOSITORY_ROOT}" REPOSITORY_ROOT)
add_compile_options(
    "-ffile-prefix-map=${REPOSITORY_ROOT}=."
    "-fmacro-prefix-map=${REPOSITORY_ROOT}=."
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-s3-touch-amoled-1-75c-brookesia)

# mcp-c-sdk 2.0.1 contains bounded strncpy calls that GCC 14 cannot prove are
# terminated.  Keep the compatibility exception local to that managed target;
# project sources retain the normal warnings-as-errors policy.
if(TARGET __idf_espressif__mcp-c-sdk AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(__idf_espressif__mcp-c-sdk PRIVATE
        -Wno-error=stringop-truncation
    )
endif()
