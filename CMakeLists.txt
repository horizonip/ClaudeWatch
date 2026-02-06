cmake_minimum_required(VERSION 3.16)
project(ClaudeWatch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings
if(MSVC)
    add_compile_options(/O2 /GL /GS- /Gy)
    add_link_options(/LTCG /OPT:REF /OPT:ICF)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/http_client.cpp
    src/parser.cpp
    src/ui.cpp
)

# Resource file
set(RESOURCES res/app.rc)

# Create Windows GUI executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${RESOURCES})

# Link Windows libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    winhttp
    gdiplus
    shlwapi
    comctl32
    crypt32
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Set subsystem to Windows
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "ClaudeWatch"
)
