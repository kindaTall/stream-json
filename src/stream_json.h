/**
 * @file stream_json.h
 * @brief Zero-malloc streaming JSON generator for embedded systems
 *
 * This library provides efficient JSON generation without dynamic memory allocation.
 * Uses a fixed buffer with callback-based streaming, allowing unlimited JSON size
 * from minimal RAM.
 *
 * Features:
 * - Zero malloc: All memory from caller-provided buffer
 * - Streaming: Auto-flush callback when buffer fills
 * - Bounded: Predictable memory usage
 * - Portable: Pure C, no platform dependencies
 * - cJSON-compatible API naming
 *
 * Example:
 *   char buffer[512];
 *   sjson_context_t ctx;
 *   sjson_Init(&ctx, buffer, sizeof(buffer), my_send_callback, user_data);
 *   sjson_AddStringToObject(&ctx, "status", "ok");
 *   sjson_AddIntToObject(&ctx, "count", 42);
 *   sjson_End(&ctx);
 */

#ifndef STREAM_JSON_H
#define STREAM_JSON_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Status codes returned by sjson functions
 */
typedef enum {
    SJSON_OK = 0,              /* Operation successful */
    SJSON_ERROR_INVALID_STATE, /* Operation not valid in current state */
    SJSON_ERROR_MAX_DEPTH,     /* Maximum nesting depth reached */
    SJSON_ERROR_BUFFER_FULL,   /* Buffer full and callback failed */
    SJSON_ERROR_INVALID_PARAM  /* Invalid parameter (NULL pointer, etc) */
} sjson_status_t;

/**
 * Callback function type for sending JSON chunks
 * @param buffer The buffer containing JSON data
 * @param length The length of data in the buffer
 * @param user_data User data passed to sjson_Init
 * @return true if sending was successful, false otherwise
 */
typedef bool (*sjson_send_callback_t)(const char *buffer, size_t length, void *user_data);

/**
 * Maximum nesting depth supported
 * Increase if deeper nesting needed (costs 2 bytes per level)
 */
#define SJSON_MAX_DEPTH 8

typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t used;
    void *user_data;
    sjson_send_callback_t send_callback;

    /* Nesting tracking */
    char depth_stack[SJSON_MAX_DEPTH + 1];  /* Closing chars: '}' or ']' (+1 for root) */
    uint8_t depth;                           /* Current depth (0 = root) */
    uint8_t max_depth;                       /* Max allowed depth */

    /* Comma tracking per depth */
    bool needs_comma[SJSON_MAX_DEPTH + 1];  /* true if next item needs ',' prefix (+1 for root) */

    /* Finalization flag */
    bool finalized;                          /* true when closed to depth 0 and flushed */
} sjson_context_t;


/* ========================================================================
 * Initialization and Finalization
 * ======================================================================== */

/**
 * Initialize streaming JSON context with root object
 * Opens { and prepares for object members
 * @param ctx Context to initialize
 * @param buffer Pre-allocated buffer for JSON generation
 * @param buffer_size Size of buffer (recommended: 512-2048 bytes)
 * @param callback Function called when buffer fills or on sjson_End()
 * @param user_data Pointer passed to callback (e.g., HTTP response handle)
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_InitObject(sjson_context_t *ctx, char *buffer, size_t buffer_size,
                                sjson_send_callback_t callback, void *user_data);

/**
 * Initialize streaming JSON context with root array
 * Opens [ and prepares for array elements
 * @param ctx Context to initialize
 * @param buffer Pre-allocated buffer for JSON generation
 * @param buffer_size Size of buffer (recommended: 512-2048 bytes)
 * @param callback Function called when buffer fills or on sjson_End()
 * @param user_data Pointer passed to callback (e.g., HTTP response handle)
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_InitArray(sjson_context_t *ctx, char *buffer, size_t buffer_size,
                               sjson_send_callback_t callback, void *user_data);

/**
 * Close current collection (object or array)
 * Automatically writes } or ] based on what's open
 * @param ctx JSON context
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_Close(sjson_context_t *ctx);

/**
 * Finalize JSON (close all open collections) and flush remaining data
 * @param ctx JSON context
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_End(sjson_context_t *ctx);

/**
 * Flush JSON buffer via callback without closing collections
 * @param ctx JSON context
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_Flush(sjson_context_t *ctx);

/* ========================================================================
 * Add Items to Object (cJSON-compatible naming)
 * ======================================================================== */

/**
 * Add string to current object
 * @param ctx JSON context
 * @param key Key name
 * @param value String value (will be escaped)
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddStringToObject(sjson_context_t *ctx, const char *key, const char *value);

/**
 * Add integer to current object
 * @param ctx JSON context
 * @param key Key name
 * @param value Integer value
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddIntToObject(sjson_context_t *ctx, const char *key, int64_t value);

/**
 * Add float to current object
 * @param ctx JSON context
 * @param key Key name
 * @param value Float value
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddFloatToObject(sjson_context_t *ctx, const char *key, float value);

/**
 * Add number (int or float) to current object
 * @param ctx JSON context
 * @param key Key name
 * @param value Number value (promoted to double)
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddNumberToObject(sjson_context_t *ctx, const char *key, double value);

/**
 * Add integer array to current object
 * @param ctx JSON context
 * @param key Key name
 * @param values Array of integers
 * @param count Number of values
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddIntArrayToObject(sjson_context_t *ctx, const char *key,
                                          const int64_t *values, size_t count);

/**
 * Add float array to current object
 * @param ctx JSON context
 * @param key Key name
 * @param values Array of floats
 * @param count Number of values
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddFloatArrayToObject(sjson_context_t *ctx, const char *key,
                                            const float *values, size_t count);

/**
 * Start nested array in current object
 * @param ctx JSON context
 * @param key Array key name
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddArrayToObject(sjson_context_t *ctx, const char *key);

/**
 * Start nested object in current object
 * @param ctx JSON context
 * @param key Object key name
 * @return SJSON_OK or SJSON_ERROR_MAX_DEPTH (not supported yet)
 */
sjson_status_t sjson_AddObjectToObject(sjson_context_t *ctx, const char *key);

/**
 * Add pre-serialized JSON to current object
 * @param ctx JSON context
 * @param key Key name
 * @param value Pre-serialized JSON string (not escaped)
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddRawToObject(sjson_context_t *ctx, const char *key, const char *value);

/* ========================================================================
 * Add Items to Array
 * ======================================================================== */

/**
 * Add integer to current array
 * @param ctx JSON context
 * @param value Integer value
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddIntToArray(sjson_context_t *ctx, int64_t value);

/**
 * Add float to current array
 * @param ctx JSON context
 * @param value Float value
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddFloatToArray(sjson_context_t *ctx, float value);

/**
 * Add string to current array
 * @param ctx JSON context
 * @param value String value
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddStringToArray(sjson_context_t *ctx, const char *value);

/**
 * Start nested object in current array
 * @param ctx JSON context
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddObjectToArray(sjson_context_t *ctx);

/**
 * Start nested array in current array
 * @param ctx JSON context
 * @return SJSON_OK or error code
 */
sjson_status_t sjson_AddArrayToArray(sjson_context_t *ctx);

#endif /* STREAM_JSON_H */