#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ERROR_NONE = 0,
    ERROR_NULL_POINTER,
    ERROR_INVALID_RECTANGLE,
    ERROR_MEMORY_ALLOCATION,
    ERROR_INVALID_INDEX,
    ERROR_INVALID_STATE,
    ERROR_THEME_NOT_INITIALIZED,
    ERROR_COMPONENT_NOT_FOUND,
    ERROR_LAYOUT_FAILED,
    ERROR_RENDER_FAILED,
    ERROR_INPUT_FAILED
} ErrorCode;

typedef struct {
    ErrorCode code;
    const char* message;
    const char* file;
    int line;
    const char* function;
} ErrorInfo;

// Global error state
extern ErrorInfo g_last_error;

// Error handling macros
#define SET_ERROR(code, msg) \
    do { \
        g_last_error.code = code; \
        g_last_error.message = msg; \
        g_last_error.file = __FILE__; \
        g_last_error.line = __LINE__; \
        g_last_error.function = __func__; \
    } while(0)

#define RETURN_IF_NULL(ptr) \
    do { \
        if (!(ptr)) { \
            SET_ERROR(ERROR_NULL_POINTER, "Null pointer passed to function"); \
            return false; \
        } \
    } while(0)

#define RETURN_IF_INVALID_RECT(rect) \
    do { \
        if ((rect).width <= 0 || (rect).height <= 0) { \
            SET_ERROR(ERROR_INVALID_RECTANGLE, "Invalid rectangle dimensions"); \
            return false; \
        } \
    } while(0)

// Error handling functions
void Error_Clear(void);
ErrorCode Error_GetLastCode(void);
const char* Error_GetLastMessage(void);
bool Error_HasError(void);
void Error_Log(const char* format, ...);

// Memory safety helpers
void* Safe_Malloc(size_t size);
void* Safe_Calloc(size_t count, size_t size);
void* Safe_Realloc(void* ptr, size_t size);
void Safe_Free(void* ptr);

#endif // ERROR_HANDLING_H
