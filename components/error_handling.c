#include "error_handling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Global error state
ErrorInfo g_last_error = {0};

void Error_Clear(void) {
    g_last_error.code = ERROR_NONE;
    g_last_error.message = NULL;
    g_last_error.file = NULL;
    g_last_error.line = 0;
    g_last_error.function = NULL;
}

ErrorCode Error_GetLastCode(void) {
    return g_last_error.code;
}

const char* Error_GetLastMessage(void) {
    return g_last_error.message ? g_last_error.message : "No error";
}

bool Error_HasError(void) {
    return g_last_error.code != ERROR_NONE;
}

void Error_Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    // Log to stderr for now - could be extended to file logging
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}

// Memory safety helpers
void* Safe_Malloc(size_t size) {
    if (size == 0) {
        SET_ERROR(ERROR_MEMORY_ALLOCATION, "Attempted to allocate 0 bytes");
        return NULL;
    }
    
    void* ptr = malloc(size);
    if (!ptr) {
        SET_ERROR(ERROR_MEMORY_ALLOCATION, "Memory allocation failed");
        return NULL;
    }
    
    return ptr;
}

void* Safe_Calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        SET_ERROR(ERROR_MEMORY_ALLOCATION, "Invalid calloc parameters");
        return NULL;
    }
    
    void* ptr = calloc(count, size);
    if (!ptr) {
        SET_ERROR(ERROR_MEMORY_ALLOCATION, "Memory allocation failed");
        return NULL;
    }
    
    return ptr;
}

void* Safe_Realloc(void* ptr, size_t size) {
    if (size == 0) {
        SET_ERROR(ERROR_MEMORY_ALLOCATION, "Attempted to reallocate to 0 bytes");
        return NULL;
    }
    
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        SET_ERROR(ERROR_MEMORY_ALLOCATION, "Memory reallocation failed");
        return NULL;
    }
    
    return new_ptr;
}

void Safe_Free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}
