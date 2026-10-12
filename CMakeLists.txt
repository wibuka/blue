cmake_minimum_required(VERSION 3.16)

project(Blue LANGUAGES C)

enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output
set(BLUE_STAGE_DIR "${CMAKE_SOURCE_DIR}/blue-out")
set(BLUE_VENDOR_DIR "${BLUE_STAGE_DIR}/vendor")
set(BLUE_CORE_DIR "${BLUE_STAGE_DIR}/core")

# Vendor libraries
add_subdirectory(vendor)

# Sources
file(GLOB SRCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(blue ${SRCS})

set_target_properties(blue PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BLUE_STAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BLUE_STAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BLUE_STAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${BLUE_STAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${BLUE_STAGE_DIR}"
)

target_link_libraries(blue PRIVATE
    vendor_lua
    vendor_luci
    vendor_miniaudio
)

# LLVM
if(WIN32)
    set(LLVM_SEARCH_ROOTS
        "$ENV{LLVM_ROOT}"
        "C:/Program Files/LLVM"
        "C:/Program Files (x86)/LLVM"
        "C:/LLVM"
        "$ENV{ProgramFiles}/LLVM"
    )

    set(LLVM_ROOT "")

    foreach(_llvm_root IN LISTS LLVM_SEARCH_ROOTS)
        if(EXISTS "${_llvm_root}/bin" AND EXISTS "${_llvm_root}/lib")
            set(LLVM_ROOT "${_llvm_root}" CACHE PATH
                "LLVM installation directory" FORCE)
            break()
        endif()
    endforeach()

    if(NOT LLVM_ROOT)
        message(FATAL_ERROR
            "LLVM not found. Set LLVM_ROOT to your LLVM installation directory."
        )
    endif()

    message(STATUS "LLVM_ROOT = ${LLVM_ROOT}")

    find_library(LLVM_C_LIB
        NAMES LLVM-C llvm-c
        PATHS "${LLVM_ROOT}/lib"
        NO_DEFAULT_PATH
    )

    if(NOT LLVM_C_LIB)
        find_library(LLVM_C_LIB NAMES LLVM-C llvm-c)
    endif()

    if(NOT LLVM_C_LIB)
        message(FATAL_ERROR
            "LLVM-C library not found. Check your LLVM installation."
        )
    endif()

    target_link_libraries(blue PRIVATE "${LLVM_C_LIB}")

    file(GLOB LLVM_RUNTIME_DLLS
        "${LLVM_ROOT}/bin/*.dll"
    )

    if(LLVM_RUNTIME_DLLS)
        add_custom_command(TARGET blue POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${LLVM_RUNTIME_DLLS}
                "$<TARGET_FILE_DIR:blue>"
            COMMENT "Copying LLVM runtime DLLs"
            VERBATIM
        )
    endif()
else()
    find_program(LLVM_CONFIG_EXECUTABLE
        NAMES llvm-config-22 llvm-config
    )

    if(NOT LLVM_CONFIG_EXECUTABLE)
        message(FATAL_ERROR
            "llvm-config not found. Install LLVM and add it to PATH."
        )
    endif()

    message(STATUS
        "LLVM_CONFIG_EXECUTABLE = ${LLVM_CONFIG_EXECUTABLE}"
    )

    execute_process(
        COMMAND "${LLVM_CONFIG_EXECUTABLE}" --libs --system-libs core
        OUTPUT_VARIABLE LLVM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE LLVM_CONFIG_RESULT
    )

    if(NOT LLVM_CONFIG_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to query LLVM libraries.")
    endif()

    separate_arguments(LLVM_LIBS)

    target_link_libraries(blue PRIVATE ${LLVM_LIBS})
endif()

# Compiler configuration
target_compile_definitions(blue PRIVATE
    $<$<CONFIG:Debug>:DEVELOP>
)

target_include_directories(blue PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/llvm"
)

if(WIN32)
    target_include_directories(blue PRIVATE
        "${LLVM_ROOT}/include"
    )
endif()

target_precompile_headers(blue PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h"
)

target_compile_options(blue PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-g>
)

# Platform
if(WIN32)
    set(BLUE_OS_DIR windows)
elseif(APPLE)
    set(BLUE_OS_DIR macos)
elseif(UNIX)
    set(BLUE_OS_DIR linux)
else()
    message(FATAL_ERROR
        "Unsupported platform: ${CMAKE_SYSTEM_NAME}"
    )
endif()

# Stage vendor libraries and resources
set(BLUE_VENDOR_LIBS
    lua
    zlib
    luci
    miniaudio
)

set(BLUE_VENDOR_COMMANDS)
set(BLUE_VENDOR_TARGETS)

foreach(vendor_lib IN LISTS BLUE_VENDOR_LIBS)
    set(vendor_target "vendor_${vendor_lib}")
    set(vendor_src_dir
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/${vendor_lib}"
    )
    set(vendor_stage_dir "${BLUE_VENDOR_DIR}/${vendor_lib}")
    set(vendor_os_dir "${vendor_stage_dir}/${BLUE_OS_DIR}")

    list(APPEND BLUE_VENDOR_COMMANDS
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${vendor_os_dir}"
    )

    if(TARGET ${vendor_target})
        list(APPEND BLUE_VENDOR_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${vendor_target}>"
                "${vendor_os_dir}/$<TARGET_FILE_NAME:${vendor_target}>"
        )

        list(APPEND BLUE_VENDOR_TARGETS "${vendor_target}")
    endif()

    # Blue module files
    file(GLOB_RECURSE vendor_blue_files CONFIGURE_DEPENDS
        RELATIVE "${vendor_src_dir}"
        "${vendor_src_dir}/*.blue"
    )

    foreach(vendor_blue IN LISTS vendor_blue_files)
        get_filename_component(vendor_blue_name
            "${vendor_blue}" NAME
        )

        list(APPEND BLUE_VENDOR_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${vendor_src_dir}/${vendor_blue}"
                "${vendor_stage_dir}/${vendor_blue_name}"
        )
    endforeach()

    # Module metadata
    if(EXISTS "${vendor_src_dir}/module.toml")
        list(APPEND BLUE_VENDOR_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${vendor_src_dir}/module.toml"
                "${vendor_stage_dir}/module.toml"
        )
    endif()

    # Vendor licenses
    foreach(license_name IN ITEMS
        LICENSE
        LICENSE.txt
        COPYING
        COPYING.txt
    )
        if(EXISTS "${vendor_src_dir}/${license_name}")
            list(APPEND BLUE_VENDOR_COMMANDS
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${vendor_src_dir}/${license_name}"
                    "${vendor_stage_dir}/${license_name}"
            )
        endif()
    endforeach()
endforeach()

add_custom_target(blue_vendor ALL
    ${BLUE_VENDOR_COMMANDS}
    VERBATIM
)

if(BLUE_VENDOR_TARGETS)
    add_dependencies(blue_vendor ${BLUE_VENDOR_TARGETS})
endif()

# Stage core library
file(GLOB BLUE_CORE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.blue"
)

set(BLUE_CORE_COMMANDS
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${BLUE_CORE_DIR}"
)

foreach(core_file IN LISTS BLUE_CORE_FILES)
    get_filename_component(core_name "${core_file}" NAME)

    list(APPEND BLUE_CORE_COMMANDS
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${core_file}"
            "${BLUE_CORE_DIR}/${core_name}"
    )
endforeach()

add_custom_target(blue_core ALL
    ${BLUE_CORE_COMMANDS}
    VERBATIM
)
