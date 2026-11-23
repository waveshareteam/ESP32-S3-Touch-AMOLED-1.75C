idf_component_register(
    SRCS
        "main.cpp"
        "display_perf_monitor.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        brookesia_app_calculator
        bsp_extra
        ButtonTest
        Crosshair
        display_power
        draw
        espressif__esp_lvgl_adapter
        Gravitysphere
        MusicPlayer
        Settings
        SpecAnalyzer
        XiaozhiApp
        storage_service
        system_status
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)

set(STORAGE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../spiffs")
set(STORAGE_STAGING_DIR "${CMAKE_BINARY_DIR}/storage_spiffs")
set(STORAGE_STAGING_STAMP "${CMAKE_BINARY_DIR}/storage_spiffs.stamp")
set(STORAGE_SOURCE_MANIFEST "${CMAKE_BINARY_DIR}/storage_spiffs_sources.txt")

file(GLOB_RECURSE STORAGE_SOURCE_FILES CONFIGURE_DEPENDS LIST_DIRECTORIES false
    "${STORAGE_SOURCE_DIR}/*")
list(SORT STORAGE_SOURCE_FILES)

# A newly added asset can carry an old timestamp (for example when it is copied
# from a reference firmware), in which case a timestamp-only dependency check
# can incorrectly leave the existing SPIFFS image untouched.  Track the full
# resource list in a generated manifest so additions/removals always restage
# the directory even when the asset mtimes predate the staging stamp.
string(JOIN "\n" STORAGE_SOURCE_MANIFEST_CONTENT ${STORAGE_SOURCE_FILES})
file(GENERATE
    OUTPUT "${STORAGE_SOURCE_MANIFEST}"
    CONTENT "${STORAGE_SOURCE_MANIFEST_CONTENT}\n"
)

# AIChats uses the complete Puhui 30 px glyph set.  Keep the multi-megabyte
# binary in SPIFFS instead of linking it into the application image, and stage
# it from the managed xiaozhi-fonts component so the source tree stays small.
idf_build_get_property(build_components BUILD_COMPONENTS)
set(XIAOZHI_FONTS_DIR "")
foreach(component IN LISTS build_components)
    if(component MATCHES "xiaozhi-fonts")
        idf_component_get_property(XIAOZHI_FONTS_DIR ${component} COMPONENT_DIR)
        break()
    endif()
endforeach()

if(NOT XIAOZHI_FONTS_DIR)
    message(FATAL_ERROR "xiaozhi-fonts component was not resolved")
endif()

set(XIAOZHI_COMMON_FONT
    "${XIAOZHI_FONTS_DIR}/cbin/font_puhui_common_30_4.bin")
if(NOT EXISTS "${XIAOZHI_COMMON_FONT}")
    message(FATAL_ERROR "xiaozhi-fonts is missing font_puhui_common_30_4.bin")
endif()

set(XIAOZHI_COMMON_FONT_STAGED
    "${STORAGE_STAGING_DIR}/xiaozhi/font.bin")

add_custom_command(
    OUTPUT "${STORAGE_STAGING_STAMP}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STORAGE_STAGING_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${STORAGE_STAGING_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${STORAGE_SOURCE_DIR}" "${STORAGE_STAGING_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${STORAGE_STAGING_DIR}/xiaozhi"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${XIAOZHI_COMMON_FONT}" "${XIAOZHI_COMMON_FONT_STAGED}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${STORAGE_STAGING_STAMP}"
    DEPENDS
        ${STORAGE_SOURCE_FILES}
        "${STORAGE_SOURCE_MANIFEST}"
        "${XIAOZHI_COMMON_FONT}"
    COMMENT "Staging Brookesia SPIFFS resources"
    VERBATIM
)
add_custom_target(storage_spiffs_stage DEPENDS "${STORAGE_STAGING_STAMP}")

spiffs_create_partition_image(
    storage "${STORAGE_STAGING_DIR}"
    FLASH_IN_PROJECT
    DEPENDS storage_spiffs_stage
)
