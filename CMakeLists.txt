cmake_minimum_required(VERSION 3.14)

project(
    CSV_READER
    VERSION 1.0.0
    DESCRIPTION "USB HOST comunication with libusb"
    LANGUAGES C
)

# =========================
# C standard
# =========================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# =========================
# Build type
# =========================

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# =========================
# Source files
# =========================

set(SOURCES
    src/main.c
)

# =========================
# Header files
# =========================

set(HEADERS
    inc/main.h
)

# =========================
# File existence checks
# =========================

foreach(FILE ${SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")
        message(FATAL_ERROR "Source file not found: ${FILE}")
    endif()
endforeach()

# =========================
# Executable
# =========================

add_executable(USB_HOST
    ${SOURCES}
    ${HEADERS}
)

# =========================
# Include directories
# =========================

# Добавляем путь к вашим локальным инклюдам и инклюдам libusb
target_include_directories(USB_HOST PRIVATE 
    inc 
    3rdparty/libusb/inc
)

# =========================
# Link libraries (Подключение Libusb)
# =========================

# Линкуем статический файл libusb и необходимые системные библиотеки Windows
target_link_libraries(USB_HOST PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libusb/lib/libusb-1.0.a"
    setupapi
    ole32
    advapi32
)

# =========================
# Compiler warnings
# =========================

target_compile_options(USB_HOST PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

# =========================
# Debug helper
# =========================

function(enable_debug target_name)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")

        target_compile_options(${target_name} PRIVATE
            -g
            -O0
            -fno-omit-frame-pointer
        )

    endif()

endfunction()

enable_debug(USB_HOST)

# =========================
# Output directories
# =========================

set_target_properties(USB_HOST PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# =========================
# Project info
# =========================

message(STATUS "Project name: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")

enable_testing()