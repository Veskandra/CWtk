cmake_minimum_required(VERSION 3.16)
project(WinTk VERSION 1.0.0 LANGUAGES C RC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "WinTk targets Windows only.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# WinTk's public API is built on UTF-8. Keep UTF-8 literals in examples and
# user code intact on both GCC/Clang and MSVC.
if(MSVC)
    add_compile_options(/utf-8)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

add_library(wintk STATIC
    src/wtk_core.c
    src/wtk_draw.c
    src/wtk_layout.c
    src/wtk_native.c
    src/wtk_widgets.c
)
add_library(WinTk::wintk ALIAS wintk)

target_include_directories(wintk
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(wintk PUBLIC UNICODE _UNICODE)
target_link_libraries(wintk PUBLIC user32 gdi32 comctl32 uxtheme)

if(MSVC)
    target_compile_options(wintk PRIVATE /W4)
else()
    target_compile_options(wintk PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The .rc file carries the common-controls v6 manifest and application icon.
set(WINTK_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/wintk.rc")

add_executable(hello WIN32
    examples/hello.c
    ${WINTK_RESOURCE}
)
target_link_libraries(hello PRIVATE WinTk::wintk)
target_include_directories(hello PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")

add_executable(wintk_gallery WIN32
    examples/gallery.c
    ${WINTK_RESOURCE}
)
target_link_libraries(wintk_gallery PRIVATE WinTk::wintk)
target_include_directories(wintk_gallery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")

add_executable(form WIN32
    examples/form.c
    ${WINTK_RESOURCE}
)
target_link_libraries(form PRIVATE WinTk::wintk)
target_include_directories(form PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")

add_executable(dashboard WIN32
    examples/dashboard.c
    ${WINTK_RESOURCE}
)
target_link_libraries(dashboard PRIVATE WinTk::wintk)
target_include_directories(dashboard PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")

add_executable(canvas WIN32
    examples/canvas.c
    ${WINTK_RESOURCE}
)
target_link_libraries(canvas PRIVATE WinTk::wintk)
target_include_directories(canvas PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")

if(MINGW)
    # Keep the examples self-contained (no libgcc_s DLL next to the .exe).
    target_link_options(hello PRIVATE -static-libgcc)
    target_link_options(wintk_gallery PRIVATE -static-libgcc)
    target_link_options(form PRIVATE -static-libgcc)
    target_link_options(dashboard PRIVATE -static-libgcc)
    target_link_options(canvas PRIVATE -static-libgcc)
endif()

# WinTk Designer is a separate subproject. It can also be configured directly:
#   cmake -S designer -B designer/build-cmake -G "MinGW Makefiles"
option(WINTK_BUILD_DESIGNER "Build the WinTk Designer subproject" ON)
if(WINTK_BUILD_DESIGNER)
    add_subdirectory(designer)
endif()
