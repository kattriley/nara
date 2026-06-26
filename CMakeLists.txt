cmake_minimum_required(VERSION 3.20)
project(nara VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  webview2
  URL      "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2903.40"
  SOURCE_DIR "${CMAKE_BINARY_DIR}/webview2"
)
FetchContent_Populate(webview2)

set(WEBVIEW2_INCLUDE_DIR "${webview2_SOURCE_DIR}/build/native/include")

set(SOURCES
  src/main.cpp
  src/main.h
  src/browser_window.cpp
  src/browser_window.h
  src/resources.rc
)

add_executable(nara WIN32 ${SOURCES})

target_include_directories(nara PRIVATE src ${WEBVIEW2_INCLUDE_DIR})
target_link_libraries(nara PRIVATE -L${CMAKE_BINARY_DIR} -lwebview2loader ole32 oleaut32 uuid)

add_custom_command(TARGET nara POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/webview2/build/native/x64/WebView2Loader.dll"
    "$<TARGET_FILE_DIR:nara>"
)

add_custom_command(TARGET nara POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/src/browser.ico"
    "$<TARGET_FILE_DIR:nara>"
)
