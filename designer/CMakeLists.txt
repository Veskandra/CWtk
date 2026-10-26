cmake_minimum_required(VERSION 3.16)
project(WinTkDesigner VERSION 1.0.0 LANGUAGES C RC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "WinTk Designer targets Windows only.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
    add_compile_options(/utf-8)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# When this directory is added by the repository root, reuse the already built
# WinTk target. When configured on its own, build a private copy of the
# framework so the designer remains a truly standalone subproject.
if(TARGET WinTk::wintk)
    set(WINTK_DESIGNER_FRAMEWORK WinTk::wintk)
else()
    add_library(wintk_designer_framework STATIC
        ../src/wtk_core.c
        ../src/wtk_draw.c
        ../src/wtk_layout.c
        ../src/wtk_native.c
        ../src/wtk_widgets.c
    )
    target_include_directories(wintk_designer_framework
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    )
    target_compile_definitions(wintk_designer_framework PUBLIC UNICODE _UNICODE)
    target_link_libraries(wintk_designer_framework
        PUBLIC user32 gdi32 comctl32 uxtheme
    )
    if(MSVC)
        target_compile_options(wintk_designer_framework PRIVATE /W4)
    else()
        target_compile_options(wintk_designer_framework
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()
    set(WINTK_DESIGNER_FRAMEWORK wintk_designer_framework)
endif()

add_executable(wintk_designer WIN32
    src/designer.c
    src/designer_model.c
    ../resources/wintk.rc
)
target_include_directories(wintk_designer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../resources"
)
target_compile_definitions(wintk_designer PRIVATE UNICODE _UNICODE)
if(MSVC)
    target_compile_options(wintk_designer PRIVATE /W4)
else()
    target_compile_options(wintk_designer PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_link_libraries(wintk_designer PRIVATE
    ${WINTK_DESIGNER_FRAMEWORK}
    comdlg32
)

if(MINGW)
    target_link_options(wintk_designer PRIVATE -static-libgcc)
endif()
