/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_COMMON_H
#define AFORC_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AFORC_VERSION_MAJOR 0
#define AFORC_VERSION_MINOR 1
#define AFORC_VERSION_PATCH 0

#if defined(_WIN32) && defined(AFORC_SHARED)
#  if defined(AFORC_BUILDING_LIBRARY)
#    define AFORC_API __declspec(dllexport)
#  else
#    define AFORC_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(AFORC_SHARED)
#  define AFORC_API __attribute__((visibility("default")))
#else
#  define AFORC_API
#endif

typedef enum AFORC_Status {
    AFORC_OK = 0,
    AFORC_ERROR_INVALID_ARGUMENT,
    AFORC_ERROR_OUT_OF_MEMORY,
    AFORC_ERROR_OVERFLOW,
    AFORC_ERROR_IO,
    AFORC_ERROR_PLATFORM,
    AFORC_ERROR_UNSUPPORTED,
    AFORC_ERROR_LIMIT,
    AFORC_ERROR_STATE,
    AFORC_ERROR_NOT_FOUND,
    AFORC_ERROR_STALE_HANDLE,
    AFORC_ERROR_EXISTS,
    AFORC_ERROR_FORMAT,
    AFORC_ERROR_CHECKSUM,
    AFORC_ERROR_END_OF_STREAM,
    AFORC_ERROR_INTERRUPTED
} AFORC_Status;

typedef struct AFORC_Error {
    AFORC_Status status;
    const char *subsystem;
    char message[192];
} AFORC_Error;

typedef void *(*AFORC_AllocFn)(void *context, size_t size);
typedef void *(*AFORC_ReallocFn)(void *context, void *memory, size_t size);
typedef void (*AFORC_FreeFn)(void *context, void *memory);

typedef struct AFORC_Allocator {
    void *context;
    AFORC_AllocFn allocate;
    AFORC_ReallocFn reallocate;
    AFORC_FreeFn deallocate;
} AFORC_Allocator;

typedef enum AFORC_LogLevel {
    AFORC_LOG_TRACE = 0,
    AFORC_LOG_DEBUG,
    AFORC_LOG_INFO,
    AFORC_LOG_WARNING,
    AFORC_LOG_ERROR
} AFORC_LogLevel;

typedef void (*AFORC_LogFn)(void *context, AFORC_LogLevel level,
                          const char *subsystem, const char *message);

typedef struct AFORC_Logger {
    void *context;
    AFORC_LogFn write;
    AFORC_LogLevel minimum_level;
} AFORC_Logger;

typedef struct AFORC_Point {
    int32_t x;
    int32_t y;
} AFORC_Point;

typedef struct AFORC_Size {
    int32_t width;
    int32_t height;
} AFORC_Size;

typedef struct AFORC_Rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} AFORC_Rect;

AFORC_API AFORC_Allocator aforc_allocator_default(void);
AFORC_API bool aforc_size_multiply(size_t left, size_t right, size_t *out);
AFORC_API bool aforc_size_add(size_t left, size_t right, size_t *out);
AFORC_API AFORC_Status aforc_alloc_array(const AFORC_Allocator *allocator,
                                   size_t count, size_t element_size,
                                   void **out_memory);
AFORC_API AFORC_Status aforc_realloc_array(const AFORC_Allocator *allocator,
                                     void *memory, size_t count,
                                     size_t element_size, void **out_memory);
AFORC_API void aforc_free(const AFORC_Allocator *allocator, void *memory);
AFORC_API void aforc_error_clear(AFORC_Error *error);
AFORC_API void aforc_error_set(AFORC_Error *error, AFORC_Status status,
                           const char *subsystem, const char *format, ...);
AFORC_API const char *aforc_status_string(AFORC_Status status);
AFORC_API void aforc_log(const AFORC_Logger *logger, AFORC_LogLevel level,
                     const char *subsystem, const char *message);
AFORC_API bool aforc_rect_contains(AFORC_Rect rect, AFORC_Point point);
AFORC_API AFORC_Rect aforc_rect_intersection(AFORC_Rect left, AFORC_Rect right);

#ifdef __cplusplus
}
#endif

#endif
