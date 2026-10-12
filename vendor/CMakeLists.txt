
# Vendor libraries
# Windows: *.lib
# Linux/macOS: lib*.a

set(VENDOR_WARN_OFF
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-w>
    $<$<C_COMPILER_ID:MSVC>:/w>
)

# Apply platform-specific static library naming.
function(set_vendor_static_name target name)
    if(WIN32)
        set_target_properties(${target} PROPERTIES
            PREFIX ""
            OUTPUT_NAME "${name}"
            SUFFIX ".lib"
        )
    else()
        set_target_properties(${target} PROPERTIES
            PREFIX "lib"
            OUTPUT_NAME "${name}"
            SUFFIX ".a"
        )
    endif()
endfunction()


# ============================================================
# Lua 5.4.8
# ============================================================

set(LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lua/lua-5.4.8/src")

file(GLOB LUA_ALL_SRCS CONFIGURE_DEPENDS
    "${LUA_DIR}/*.c"
)

list(REMOVE_ITEM LUA_ALL_SRCS
    "${LUA_DIR}/lua.c"
    "${LUA_DIR}/luac.c"
)

add_library(vendor_lua STATIC
    ${LUA_ALL_SRCS}
    "${LUA_DIR}/lua.h"
    "${LUA_DIR}/luaconf.h"
)

add_library(lua::vendor ALIAS vendor_lua)

target_include_directories(vendor_lua SYSTEM PUBLIC
    "${LUA_DIR}"
)

target_compile_options(vendor_lua PRIVATE
    ${VENDOR_WARN_OFF}
    $<$<C_COMPILER_ID:MSVC>:/wd4996>
)

set_target_properties(vendor_lua PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

set_vendor_static_name(vendor_lua lua)

if(WIN32)
    target_compile_definitions(vendor_lua PRIVATE
        LUA_USE_WINDOWS
    )
elseif(APPLE)
    target_compile_definitions(vendor_lua PRIVATE
        LUA_USE_MACOSX
    )

    find_package(Threads REQUIRED)

    target_link_libraries(vendor_lua PUBLIC
        Threads::Threads
        m
    )
else()
    target_compile_definitions(vendor_lua PRIVATE
        LUA_USE_LINUX
    )

    find_package(Threads REQUIRED)

    target_link_libraries(vendor_lua PUBLIC
        Threads::Threads
        ${CMAKE_DL_LIBS}
        m
    )
endif()


# ============================================================
# LUCI
# ============================================================

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/luci/luci-0.1.0"
    EXCLUDE_FROM_ALL
)

add_library(vendor_luci ALIAS luci)
add_library(luci::vendor ALIAS luci)

target_compile_options(luci PRIVATE
    ${VENDOR_WARN_OFF}
)

set_vendor_static_name(luci luci)


# ============================================================
# miniaudio
# ============================================================

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/miniaudio-0.11.25"
)

add_library(vendor_miniaudio ALIAS miniaudio)
add_library(miniaudio::vendor ALIAS miniaudio)

set_vendor_static_name(miniaudio miniaudio)
