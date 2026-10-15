cmake_minimum_required(VERSION 3.20)
project(GDDS-Tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8 /W4 /EHsc /O2 /permissive-)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/DirectStorage/include
)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/DirectStorage/lib/x64
)

add_executable(gdds_tool
    src/main.cpp
)

target_link_libraries(gdds_tool PRIVATE
    dstorage.lib
)

# Post-build: copy DirectStorage runtime DLLs alongside executable
add_custom_command(TARGET gdds_tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/DirectStorage/lib/x64/dstorage.dll"
        "$<TARGET_FILE_DIR:gdds_tool>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/DirectStorage/lib/x64/dstoragecore.dll"
        "$<TARGET_FILE_DIR:gdds_tool>"
)
