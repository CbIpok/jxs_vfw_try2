cmake_minimum_required(VERSION 3.15)
project(VFWFFmpegWrapper VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build VFW codec library
add_library(vfw_ffmpeg_wrapper SHARED
        src/vfw_wrapper.cpp
)
target_include_directories(vfw_ffmpeg_wrapper PRIVATE src)
if(WIN32)
    target_compile_definitions(vfw_ffmpeg_wrapper PRIVATE UNICODE _UNICODE)
    target_link_libraries(vfw_ffmpeg_wrapper PRIVATE Shlwapi)
endif()
set_target_properties(vfw_ffmpeg_wrapper PROPERTIES
        PREFIX ""
        OUTPUT_NAME "jxs_ffmpeg_vfw"
)

# Build test application
add_executable(test_vfw src/test.cpp)
target_link_libraries(test_vfw vfw32)
