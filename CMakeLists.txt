cmake_minimum_required(VERSION 3.10)
project(muxkit VERSION 0.4.3)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(_DEFAULT_SOURCE)

# 检测目标架构
if(NOT DEFINED TARGET_ARCH)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE TARGET_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# Debug 构建时启用日志
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DENABLE_LOG)
endif()

# 架构名称映射
if(TARGET_ARCH STREQUAL "x86_64")
    set(ARCH_NAME "amd64")
elseif(TARGET_ARCH STREQUAL "aarch64")
    set(ARCH_NAME "arm64")
else()
    set(ARCH_NAME ${TARGET_ARCH})
endif()

message(STATUS "Building for architecture: ${ARCH_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")

# libvterm sources
set(VTERM_SOURCES
        vendor/vterm/vterm.c
        vendor/vterm/encoding.c
        vendor/vterm/keyboard.c
        vendor/vterm/mouse.c
        vendor/vterm/parser.c
        vendor/vterm/pen.c
        vendor/vterm/screen.c
        vendor/vterm/state.c
        vendor/vterm/unicode.c
)

# muxkit sources - 按模块组织
set(SOURCES
        # Core
        src/core/main.c
        # Client
        src/client/client.c
        # Server
        src/server/server.c
        src/server/spawn.c
        # UI
        src/ui/window.c
        src/ui/render.c
        src/ui/input.c
        # Common
        src/common/util.c
        src/common/log.c
        src/common/i18n.c
        src/common/keyboard.c
)

# 设置输出文件名：muxkit-版本-架构[-debug]
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OUTPUT_NAME "muxkit-${PROJECT_VERSION}-${ARCH_NAME}-debug")
else()
    set(OUTPUT_NAME "muxkit-${PROJECT_VERSION}-${ARCH_NAME}")
endif()

add_executable(${OUTPUT_NAME} ${SOURCES} ${VTERM_SOURCES})

# 添加 include 目录
target_include_directories(${OUTPUT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/vterm
)

# 同时创建一个不带版本的符号链接（可选）
add_custom_command(TARGET ${OUTPUT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${OUTPUT_NAME} muxkit
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Creating symlink: muxkit -> ${OUTPUT_NAME}"
)
