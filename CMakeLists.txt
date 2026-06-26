cmake_minimum_required(VERSION 3.20)
project(nara VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
  src/main.cpp
  src/main.h
  src/resources.rc
)

add_executable(nara WIN32 ${SOURCES})

target_include_directories(nara PRIVATE src)
target_link_libraries(nara PRIVATE gdi32 ole32 shell32 comctl32)

add_custom_command(TARGET nara POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/src/browser.ico"
    "$<TARGET_FILE_DIR:nara>"
)
