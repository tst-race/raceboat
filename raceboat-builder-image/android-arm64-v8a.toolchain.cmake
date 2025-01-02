# Android Linux arm64-v8a, API v29
#
# Set required variables then include the NDK-provided toolchain file

set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM 29)
set(ANDROID_STL c++_shared)
set(ANDROID_SO_UNDEFINED true)
set(MIN_SDK_VERSION 29)

set(CMAKE_CXX_FLAGS "-DANDROID_STL=c++_shared -frtti ${CMAKE_CXX_FLAGS}")
set(CMAKE_PREFIX_PATH /android/arm64-v8a)
set(CMAKE_FIND_ROOT_PATH /android/arm64-v8a)

set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES /android/arm64-v8a/include)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES /android/arm64-v8a/include)
set(CMAKE_C_IMPLICIT_LINK_DIRECTORIES /android/arm64-v8a/lib)
set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES /android/arm64-v8a/lib)

include(/opt/android/ndk/default/build/cmake/android.toolchain.cmake)