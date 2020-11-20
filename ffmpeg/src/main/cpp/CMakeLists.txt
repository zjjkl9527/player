# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.2)


project("ffmpeg")

include_directories(third_party/ffmpeg)

add_library(
        native-lib
        SHARED
        native-lib.cpp)


add_library(
        ffmpeg
        SHARED
        IMPORTED )
set_target_properties(
        ffmpeg
        PROPERTIES IMPORTED_LOCATION
        ../../../../libs/${CMAKE_ANDROID_ARCH_ABI}/libffmpeg.so)



find_library(
              log-lib
              log )


target_link_libraries(
        native-lib
        ffmpeg
        ${log-lib} )