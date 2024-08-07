cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(uhk)

target_include_directories(app PUBLIC
    src
    src/legacy
    src/legacy/config_parser
    src/shared
)

file(GLOB usb_sources src/usb/*.c)
file(GLOB usb_cpp_sources src/usb/*.cpp)
file(GLOB_RECURSE keyboard_sources src/keyboard/*.c)
file(GLOB app_sources src/*.c)
file(GLOB app_cpp_sources src/*.cpp)
file(GLOB lvgl_dummy src/lvgl/*.c)
file(GLOB_RECURSE legacy_sources src/legacy/*.c)
file(GLOB_RECURSE shared_sources src/shared/*.c)

# add the versions.c source file conditionally
if(NOT NOVERSIONS)
    target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared/versions.c)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../shared/versions.c PROPERTIES GENERATED TRUE)
endif()

if(BOARD STREQUAL "uhk-60-right")
    file(GLOB main src/uhk60/main.c)
    target_sources(app PRIVATE ${main})
elseif ((BOARD STREQUAL "uhk-dongle"))
    target_sources(app PRIVATE ${app_sources} ${app_cpp_sources} ${usb_sources} ${usb_cpp_sources} ${legacy_sources} ${shared_sources})
else()
    target_sources(app PRIVATE ${app_sources} ${app_cpp_sources} ${usb_sources} ${usb_cpp_sources} ${legacy_sources} ${shared_sources} ${keyboard_sources} ${lvgl_dummy})
endif()

if(NOT BOARD STREQUAL "uhk-60-right")
    # C headers compiled with C++ give pointer conversion errors, turn them to warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

    set(C2USB_PATH "c2usb")

    add_subdirectory(${C2USB_PATH})

    # Link app to c2usb
    target_link_libraries(app PRIVATE
        c2usb
    )
endif()

# Adding config.bin to the final image
if (TRUE)
    # create linker script section for user configuration from DT information
    dt_nodelabel(USER_CONFIG_PARTITION_PATH NODELABEL "user_config_partition")
    dt_reg_addr(USER_CONFIG_PARTITION_ADDRESS PATH ${USER_CONFIG_PARTITION_PATH})
    dt_reg_size(USER_CONFIG_PARTITION_SIZE PATH ${USER_CONFIG_PARTITION_PATH})

    zephyr_linker_memory(NAME USER_CONFIG
        FLAGS r
        START ${USER_CONFIG_PARTITION_ADDRESS}
        SIZE ${USER_CONFIG_PARTITION_SIZE}
    )
    zephyr_linker_section(NAME .userconfig GROUP USER_CONFIG)

    # the assembly file places the binaries in the correct memory regions
    set(CONFIG_BIN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config.bin")
    set(CONFIGS_ASM_FILE "${CMAKE_BINARY_DIR}/configs_assembly.S")
    file(WRITE "${CONFIGS_ASM_FILE}" "
        .section .userconfig
        .incbin \"${CONFIG_BIN_PATH}\"
    ")
    set_source_files_properties(${CONFIGS_ASM_FILE} PROPERTIES GENERATED TRUE)

    # add it to the final elf image target
    target_sources(${logical_target_for_zephyr_elf} PRIVATE ${CONFIGS_ASM_FILE})
endif()
