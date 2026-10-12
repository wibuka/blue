cmake_minimum_required(VERSION 3.10)
project(miniaudio VERSION 0.11.25 LANGUAGES C)

option(MINIAUDIO_BUILD_SHIM "Build the allocation shim (miniaudio_shim.c)" ON)

add_library(miniaudio STATIC
    miniaudio.c
    miniaudio.h
    $<$<BOOL:${MINIAUDIO_BUILD_SHIM}>:miniaudio_shim.c>
)
add_library(miniaudio::vendor ALIAS miniaudio)

target_include_directories(miniaudio SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(miniaudio PRIVATE ${VENDOR_WARN_OFF})
set_target_properties(miniaudio PROPERTIES OUTPUT_NAME miniaudio)

if(WIN32)
    target_compile_definitions(miniaudio PRIVATE _CRT_SECURE_NO_WARNINGS
                                               WIN32_LEAN_AND_MEAN
                                               NOMINMAX)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
