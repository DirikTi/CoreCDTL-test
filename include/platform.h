#pragma once

// Platform detection
#if  defined(__linux__) || defined(__unix__)
    #define OS_WINDOWS 0
    #define OS_LINUX 1
    #define OS_DARWIN 0
#elif defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #define OS_LINUX  0
    #define OS_DARWIN 0
#elif defined(__APPLE__)
    #define OS_WINDOWS 0
    #define OS_LINUX 0
    #define OS_DARWIN 1
#else
    #error "Unsupported platform"
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)       __builtin_expect(!!(x), 1)
    #define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
    #define LIKELY(x)       (x)
    #define UNLIKELY(x)     (x)
#else
    #define LIKELY(x)       (x)
    #define UNLIKELY(x)     (x)
#endif

// Platform-specific definitions
#if OS_LINUX
    #define PLUGIN_LIB_EXTENSION ".so"
    #define PATH_SEPARATOR "/"
#elif OS_WINDOWS
    #define PLUGIN_LIB_EXTENSION ".dll"
    #define PATH_SEPARATOR "\\"
#elif OS_DARWIN
    #define PLUGIN_LIB_EXTENSION ".dylib"
    #define PATH_SEPARATOR "/"
#endif

// Platform-specific function macros
#if OS_LINUX || OS_DARWIN
    #define DLL_EXPORT __attribute__((visibility("default")))
    #define DLL_IMPORT
    
#elif OS_WINDOWS
    #define DLL_EXPORT __declspec(dllexport)
    #define DLL_IMPORT __declspec(dllimport)
#endif

// Memory alignment
#define CACHE_LINE_SIZE 64

// Endianness detection
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define CORE_BIG_ENDIAN 1
#else
    #define CORE_BIG_ENDIAN 0
#endif
