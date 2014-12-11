cmake_minimum_required(VERSION 2.8.4)

project(curl_rtsp)

set(SOURCE_FILES main.c)

include_directories(${PROJECT_SOURCE_DIR}/deps "/usr/local/include" "/usr/local/opt/curl/include")

link_directories("/usr/local/lib" "/usr/local/opt/curl/lib")

add_executable(curl_rtsp ${SOURCE_FILES})

target_link_libraries(curl_rtsp "libcurl.a")
