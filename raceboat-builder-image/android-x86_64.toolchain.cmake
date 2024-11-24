# Android Linux x86_64, API v29
#
# Set required variables then include the NDK-provided toolchain file

set(ANDROID_ABI x86_64)
set(ANDROID_PLATFORM 29)
set(ANDROID_STL c++_shared)
set(ANDROID_SO_UNDEFINED true)
set(MIN_SDK_VERSION 29)

set(CMAKE_CXX_FLAGS "-DANDROID_STL=c++_shared -frtti ${CMAKE_CXX_FLAGS}")
set(CMAKE_PREFIX_PATH /android/x86_64)
set(CMAKE_FIND_ROOT_PATH /android/x86_64)

set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES /android/x86_64/include)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES /android/x86_64/include)
set(CMAKE_C_IMPLICIT_LINK_DIRECTORIES /android/x86_64/lib)
set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES /android/x86_64/lib)

include(/opt/android/ndk/default/build/cmake/android.toolchain.cmake)
