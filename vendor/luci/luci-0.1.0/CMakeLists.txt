cmake_minimum_required(VERSION 3.12)

project(luci C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# libluci.a is meant to be dropped into an application as a single archive, so
# its objects must be position independent to stay usable in PIE executables
# and shared libraries.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# FreeType is bundled, static, and built with every optional codec disabled so
# that it pulls in no library of its own. There is nothing to install: libluci.a
# stays in the build tree to be copied by hand, so upstream install rules are
# silenced to keep them from pulling FreeType headers and a redundant
# libfreetype.a into the install tree. Its sources are compiled into OBJECT
# files below and archived straight into libluci.a instead.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
if(WIN32)
  enable_language(RC) # FreeType ships a resource file on Windows
endif()
add_subdirectory(external/freetype EXCLUDE_FROM_ALL)

# Recompile the sources of a static library target as an OBJECT library,
# inheriting its sources, include directories and definitions so the two can
# never drift apart. Used for dependencies that own their build system.
function(luci_objects_from_static out_var upstream)
  get_target_property(_type ${upstream} TYPE)
  if(NOT _type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "'${upstream}' must be a static library target, got '${_type}'")
  endif()

  get_target_property(_source_dir ${upstream} SOURCE_DIR)
  get_target_property(_sources ${upstream} SOURCES)
  set(_compilable "")
  foreach(_source IN LISTS _sources)
    if(NOT IS_ABSOLUTE "${_source}")
      set(_source "${_source_dir}/${_source}")
    endif()
    if(_source MATCHES "\\.(c|m|mm|cc|cpp|cxx|S|rc)$")
      list(APPEND _compilable "${_source}")
    endif()
  endforeach()
  if(NOT _compilable)
    message(FATAL_ERROR "no compilable sources found on target '${upstream}'")
  endif()

  get_target_property(_includes ${upstream} INCLUDE_DIRECTORIES)
  get_target_property(_defines ${upstream} COMPILE_DEFINITIONS)
  get_target_property(_options ${upstream} COMPILE_OPTIONS)

  add_library(${out_var} OBJECT ${_compilable})
  target_include_directories(${out_var} PRIVATE ${_includes})
  target_compile_definitions(${out_var} PRIVATE ${_defines})
  if(_options)
    target_compile_options(${out_var} PRIVATE ${_options})
  endif()
  if(NOT WIN32)
    set_target_properties(${out_var} PROPERTIES C_VISIBILITY_PRESET hidden)
  endif()
endfunction()

luci_objects_from_static(freetype_objects freetype)

# glad loads libGL / opengl32 at runtime through dlopen / LoadLibrary, so it has
# no link time dependency of its own.
add_library(glad_objects OBJECT external/glad/src/glad.c)
target_include_directories(glad_objects PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/external
  ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)
# glad does (void* -> func*) casts via loader; ISO C pedantic warning is expected — silence for this external target
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(glad_objects PRIVATE -Wno-pedantic)
endif()

# RGFW and stb_image are single header libraries compiled directly by luci.c,
# so they are already part of the archive and need no target of their own.

# Main library: LUCI plus every LUCI owned static dependency as plain objects.
# Nothing else is linked privately, so no internal target can reach consumers.
add_library(luci STATIC
  luci.c
  $<TARGET_OBJECTS:freetype_objects>
  $<TARGET_OBJECTS:glad_objects>
)

get_target_property(LUCI_FREETYPE_INCLUDES freetype_objects INCLUDE_DIRECTORIES)
target_include_directories(luci
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
    ${LUCI_FREETYPE_INCLUDES}
)

# Only unavoidable OS libraries are public: the windowing backend RGFW calls
# into, libm, and the dynamic loader glad uses to open libGL.
if(UNIX AND NOT APPLE)
  target_link_libraries(luci PUBLIC X11 Xrandr m ${CMAKE_DL_LIBS})
elseif(WIN32)
  target_link_libraries(luci PUBLIC gdi32)
elseif(APPLE)
  target_link_libraries(luci PUBLIC
    "-framework Cocoa" "-framework CoreVideo" "-framework IOKit")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
  add_subdirectory(examples)
endif()
