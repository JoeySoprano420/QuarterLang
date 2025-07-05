cmake_minimum_required(VERSION 3.15)
project(QuarterLangUnifiedRuntime)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    main.cpp
    QuarterLang_Compiler_Pipeline.cpp
)

# Header-only modules
include_directories(
    ${CMAKE_SOURCE_DIR}
)

# Executable
add_executable(qtr_demo ${SOURCES})
