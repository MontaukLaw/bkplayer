cmake_minimum_required(VERSION 3.22.1)
project("bkplayer")

set(FFMPEG ${CMAKE_SOURCE_DIR}/ffmpeg)  # ffmpeg的路径
set(RKNN ${CMAKE_SOURCE_DIR}/rknn)

include_directories(${CMAKE_SOURCE_DIR}/include)  # 导入include文件夹
include_directories(${RKNN}/include)  # 导入rknn的头文件
include_directories(${FFMPEG}/include)  # 导入ffmpeg的头文件
include_directories(${OPENCV}/include)

file(GLOB cpp_src_file *.cpp)

add_library(${CMAKE_PROJECT_NAME} SHARED
        native-lib.cpp
        ${cpp_src_file}
        )

target_link_libraries(${CMAKE_PROJECT_NAME}
        android
        ${RKNN}/libs/${ANDROID_ABI}/librknnrt.so

        ${FFMPEG}/libs/${ANDROID_ABI}/libavcodec.so
        ${FFMPEG}/libs/${ANDROID_ABI}/libavdevice.so
        ${FFMPEG}/libs/${ANDROID_ABI}/libavfilter.so
        ${FFMPEG}/libs/${ANDROID_ABI}/libavutil.so
        ${FFMPEG}/libs/${ANDROID_ABI}/libavformat.so
        ${FFMPEG}/libs/${ANDROID_ABI}/libswresample.so
        ${FFMPEG}/libs/${ANDROID_ABI}/libswscale.so
        z # 引入z库
        EGL
        GLESv2
        android # 引入android库
        OpenSLES # 引入OpenSLES库
        log)