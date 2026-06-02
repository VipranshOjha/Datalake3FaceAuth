# # CMakeLists.txt — Android NDK Build Configuration
# Datalake 3.0 Face Authentication Native Module
# Hackathon 7.0 | NHAI

# OUTPUT: libdatalake_faceauth.so
# ABI:    armeabi-v7a, arm64-v8a
# STD:    C++17
#
# This CMake file compiles the shared C++ engine and links it with:
#   - React Native JSI runtime
#   - ONNX Runtime Mobile (placeholder)
#   - SQLite amalgamation (static)
#   - nanopb (embedded, static)
cmake_minimum_required(VERSION 3.13)
project(datalake_faceauth LANGUAGES CXX C)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compile Flags
# -O2:             Aggressive optimization for mobile performance
# -frtti:          Required for JSI HostObject (uses dynamic_cast)
# -fno-exceptions: Disabled for binary size + performance
#                  (use error codes instead)
# -DANDROID:       Platform detection macro
add_compile_options(
    -O2
    -frtti
    -fno-exceptions
    -DANDROID
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Find React Native Packages
find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)

# Shared C++ Source Files
# Collect all .cpp files from the shared C++ engine directory.
# EXCLUDE iOS-specific platform handler to prevent compilation errors.
set(CPP_DIR "${CMAKE_SOURCE_DIR}/../cpp")

file(GLOB_RECURSE CPP_SOURCES
    "${CPP_DIR}/*.cpp"
)

# Remove iOS-specific files from Android build
list(FILTER CPP_SOURCES EXCLUDE REGEX ".*PlatformImageHandler_iOS\\.cpp$")

# JNI Entry Point
set(JNI_SOURCES
    "${CMAKE_SOURCE_DIR}/src/main/cpp/jni_entry.cpp"
)

# SQLite Amalgamation (Static Library)
# Building SQLite as a separate static lib keeps compile flags isolated
# and allows -fno-exceptions on the main code while SQLite uses its own.
set(SQLITE_DIR "${CPP_DIR}/third_party")

add_library(sqlite3_static STATIC
    "${SQLITE_DIR}/sqlite3.c"
)
target_include_directories(sqlite3_static PUBLIC
    "${SQLITE_DIR}"
)
# SQLite needs these for proper Android operation
target_compile_definitions(sqlite3_static PRIVATE
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_THREADSAFE=2
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_LIKE_DOESNT_MATCH_BLOBS
    SQLITE_OMIT_DECLTYPE
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_USE_ALLOCA
)

# nanopb (Static Library)
set(NANOPB_DIR "${CPP_DIR}/third_party/nanopb")

add_library(nanopb_static STATIC
    "${NANOPB_DIR}/pb_encode.c"
    "${NANOPB_DIR}/pb_decode.c"
    "${NANOPB_DIR}/pb_common.c"
)
target_include_directories(nanopb_static PUBLIC
    "${NANOPB_DIR}"
)

# ONNX Runtime Mobile (Placeholder)
# Uncomment and configure when ONNX Runtime .so files are downloaded.
# Download script: scripts/download_onnxruntime.sh
#
# set(ORT_DIR "${CMAKE_SOURCE_DIR}/libs/onnxruntime")
#
# add_library(onnxruntime SHARED IMPORTED)
# set_target_properties(onnxruntime PROPERTIES
#     IMPORTED_LOCATION "${ORT_DIR}/jni/${ANDROID_ABI}/libonnxruntime.so"
# )
# target_include_directories(datalake_faceauth PRIVATE
#     "${ORT_DIR}/headers"
# )

# Main Shared Library
add_library(datalake_faceauth SHARED
    ${CPP_SOURCES}
    ${JNI_SOURCES}
)

# Include Directories
target_include_directories(datalake_faceauth PRIVATE
    "${CPP_DIR}"
    "${CPP_DIR}/third_party"
    "${CPP_DIR}/third_party/nanopb"
    "${CPP_DIR}/third_party/onnxruntime"
    "${CPP_DIR}/proto"
    "${CMAKE_SOURCE_DIR}/src/main/cpp"
)

# Link Libraries
target_link_libraries(datalake_faceauth
    # React Native JSI runtime
    ReactAndroid::jsi
    ReactAndroid::react_nativemodule_core
    ReactAndroid::turbomodulejsijni

    # Facebook JNI bindings
    fbjni::fbjni

    # SQLite (static, no dynamic linking overhead)
    sqlite3_static

    # nanopb (static)
    nanopb_static

    # Android system libraries
    android
    log

    # ONNX Runtime (uncomment when available)
    # onnxruntime
)

# Strip Debug Symbols in Release
# Reduces .so size significantly for production builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(datalake_faceauth PROPERTIES
        LINK_FLAGS "-Wl,--gc-sections -Wl,--strip-all"
    )
endif()
