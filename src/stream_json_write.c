/**
 * @file stream_json_write.c
 * @brief Implementation of streaming JSON writer
 *
 * Zero-malloc JSON generator using fixed buffer with streaming callback.
 * Uses depth stack for nesting and comma tracking per depth level.
 */
#include "stream_json.h"
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Internal Helper Functions
 * ======================================================================== */
static sjson_status_t write(sjson_context_t *ctx, const char *data, size_t len)
{
    if (!ctx || !data)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    if (ctx->buffer_size == ctx->used)
    {
        sjson_status_t status = sjson_Flush(ctx);
        if (status != SJSON_OK)
        {
            return status;
        }
    }

    while (len > 0)
    {
        size_t available = ctx->buffer_size - ctx->used;
        size_t to_write = (len < available) ? len : available; // > 0 since we checked len > 0 and available > 0 because we checked before

        memcpy(ctx->buffer + ctx->used, data, to_write);
        ctx->used += to_write;
        data += to_write;
        len -= to_write;

        // If buffer is full flush it, independent of remaining len
        if (ctx->used == ctx->buffer_size)
        {
            sjson_status_t status = sjson_Flush(ctx);
            if (status != SJSON_OK)
            {
                return status;
            }
        }
    }

    return SJSON_OK;
}

static sjson_status_t write_str(sjson_context_t *ctx, const char *str)
{
    return write(ctx, str, strlen(str));
}

static sjson_status_t write_char(sjson_context_t *ctx, char c)
{
    return write(ctx, &c, 1);
}

/* Write comma if needed before next item at current depth */
static sjson_status_t add_comma_if_needed(sjson_context_t *ctx)
{
    if (ctx->needs_comma[ctx->depth])
    {
        sjson_status_t status = write_char(ctx, ',');
        if (status != SJSON_OK)
            return status;
    }
    ctx->needs_comma[ctx->depth] = true; // Next item will need comma
    return SJSON_OK;
}

/* Check if in valid object state (for AddXToObject functions) */
static sjson_status_t check_object_state(sjson_context_t *ctx)
{
    if (!ctx)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    if (ctx->finalized)
    {
        return SJSON_ERROR_INVALID_STATE;
    }

    // Must be in an object (depth > 0 and top of stack is '}')
    if (ctx->depth == 0 || ctx->depth_stack[ctx->depth - 1] != '}')
    {
        return SJSON_ERROR_INVALID_STATE;
    }

    return SJSON_OK;
}

/* Check if in valid array state (for AddXToArray functions) */
static sjson_status_t check_array_state(sjson_context_t *ctx)
{
    if (!ctx)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    if (ctx->finalized)
    {
        return SJSON_ERROR_INVALID_STATE;
    }

    // Must be in an array (depth > 0 and top of stack is ']')
    if (ctx->depth == 0 || ctx->depth_stack[ctx->depth - 1] != ']')
    {
        return SJSON_ERROR_INVALID_STATE;
    }

    return SJSON_OK;
}

/* ========================================================================
 * Public API - Initialization
 * ======================================================================== */

sjson_status_t sjson_InitObject(sjson_context_t *ctx, char *buffer, size_t buffer_size,
                                sjson_send_callback_t callback, void *user_data)
{
    if (!ctx || !buffer || buffer_size == 0 || !callback)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    ctx->buffer = buffer;
    ctx->buffer_size = buffer_size;
    ctx->used = 0;
    ctx->user_data = user_data;
    ctx->send_callback = callback;
    ctx->depth = 0;
    ctx->max_depth = SJSON_MAX_DEPTH; // Allow 1 level of nesting by default
    ctx->finalized = false;

    // Initialize stacks
    memset(ctx->depth_stack, 0, sizeof(ctx->depth_stack));
    memset(ctx->needs_comma, 0, sizeof(ctx->needs_comma));

    // Start root object
    sjson_status_t status = write_char(ctx, '{');
    if (status != SJSON_OK)
        return status;

    ctx->depth_stack[ctx->depth] = '}'; // Will close with '}'
    ctx->depth++;
    ctx->needs_comma[ctx->depth] = false;

    return SJSON_OK;
}

sjson_status_t sjson_InitArray(sjson_context_t *ctx, char *buffer, size_t buffer_size,
                               sjson_send_callback_t callback, void *user_data)
{
    if (!ctx || !buffer || buffer_size == 0 || !callback)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    ctx->buffer = buffer;
    ctx->buffer_size = buffer_size;
    ctx->used = 0;
    ctx->user_data = user_data;
    ctx->send_callback = callback;
    ctx->depth = 0;
    ctx->max_depth = SJSON_MAX_DEPTH; // Allow 1 level of nesting by default
    ctx->finalized = false;

    // Initialize stacks
    memset(ctx->depth_stack, 0, sizeof(ctx->depth_stack));
    memset(ctx->needs_comma, 0, sizeof(ctx->needs_comma));

    // Start root array
    sjson_status_t status = write_char(ctx, '[');
    if (status != SJSON_OK)
        return status;

    ctx->depth_stack[ctx->depth] = ']'; // Will close with ']'
    ctx->depth++;
    ctx->needs_comma[ctx->depth] = false;

    return SJSON_OK;
}

sjson_status_t sjson_Close(sjson_context_t *ctx)
{
    if (!ctx)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    if (ctx->finalized || ctx->depth == 0)
    {
        return SJSON_ERROR_INVALID_STATE;
    }

    // Pop from stack and write closing char
    ctx->depth--;
    sjson_status_t status = write_char(ctx, ctx->depth_stack[ctx->depth]);
    if (status != SJSON_OK)
    {
        ctx->depth++; // Restore depth on failure
        return status;
    }

    // If we just closed root collection, finalize
    if (ctx->depth == 0)
    {
        ctx->finalized = true;
        status = sjson_Flush(ctx);
        if (status != SJSON_OK)
        {
            return status;
        }
    }
    else
    {
        // Parent now has an element
        ctx->needs_comma[ctx->depth] = true;
    }

    return SJSON_OK;
}

sjson_status_t sjson_End(sjson_context_t *ctx)
{
    if (!ctx)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    if (ctx->finalized)
    {
        return sjson_Flush(ctx);
    }

    // Close all open collections using sjson_Close
    // Last close will auto-flush and finalize when depth hits 0
    while (ctx->depth > 0)
    {
        sjson_status_t status = sjson_Close(ctx);
        if (status != SJSON_OK)
        {
            return status; // sjson_Close already restored state on failure
        }
    }

    return SJSON_OK;
}

sjson_status_t sjson_Flush(sjson_context_t *ctx)
{
    if (ctx->used == 0)
    {
        return SJSON_OK;
    }

    bool result = ctx->send_callback(ctx->buffer, ctx->used, ctx->user_data);
    if (!result)
    {
        return SJSON_ERROR_BUFFER_FULL;
    }
    ctx->used = 0;
    return SJSON_OK;
}


/* ========================================================================
 * Add to Object
 * Note: All Add functions check finalized flag to prevent use after close
 * ======================================================================== */

sjson_status_t sjson_AddStringToObject(sjson_context_t *ctx, const char *key, const char *value)
{
    if (!ctx || !key || !value)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_object_state(ctx);
    if (status != SJSON_OK)
        return status;

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    // Write: "key":"value"
    char buffer[256];
    int written = snprintf(buffer, sizeof(buffer), "\"%s\":\"%s\"", key, value);

    if (written < 0 || written >= (int)sizeof(buffer))
    {
        return SJSON_ERROR_BUFFER_FULL;
    }

    return write_str(ctx, buffer);
}

sjson_status_t sjson_AddIntToObject(sjson_context_t *ctx, const char *key, int64_t value)
{
    if (!ctx || !key)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_object_state(ctx);
    if (status != SJSON_OK)
        return status;

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    // Write: "key":value
    char buffer[128];
    int written = snprintf(buffer, sizeof(buffer), "\"%s\":%ld", key, (long)value);

    if (written < 0 || written >= (int)sizeof(buffer))
    {
        return SJSON_ERROR_BUFFER_FULL;
    }

    return write_str(ctx, buffer);
}

sjson_status_t sjson_AddFloatToObject(sjson_context_t *ctx, const char *key, float value)
{
    if (!ctx || !key)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_object_state(ctx);
    if (status != SJSON_OK)
        return status;

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    // Write: "key":value
    char buffer[128];
    int written = snprintf(buffer, sizeof(buffer), "\"%s\":%.6f", key, value);

    if (written < 0 || written >= (int)sizeof(buffer))
    {
        return SJSON_ERROR_BUFFER_FULL;
    }

    return write_str(ctx, buffer);
}

sjson_status_t sjson_AddNumberToObject(sjson_context_t *ctx, const char *key, double value)
{
    return sjson_AddFloatToObject(ctx, key, (float)value);
}

sjson_status_t sjson_AddIntArrayToObject(sjson_context_t *ctx, const char *key,
                                         const int64_t *values, size_t count)
{
    if (!ctx || !key || !values)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_object_state(ctx);
    if (status != SJSON_OK)
        return status;

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    // Write: "key":[
    char buffer[64];
    int written = snprintf(buffer, sizeof(buffer), "\"%s\":[", key);

    if (written < 0 || written >= (int)sizeof(buffer))
    {
        return SJSON_ERROR_BUFFER_FULL;
    }

    status = write_str(ctx, buffer);
    if (status != SJSON_OK)
        return status;

    // Add array elements
    for (size_t i = 0; i < count; i++)
    {
        char value_buffer[32];
        written = snprintf(value_buffer, sizeof(value_buffer), "%s%ld",
                           i > 0 ? "," : "", (long)values[i]);

        if (written < 0 || written >= (int)sizeof(value_buffer))
        {
            return SJSON_ERROR_BUFFER_FULL;
        }

        status = write_str(ctx, value_buffer);
        if (status != SJSON_OK)
            return status;
    }

    // Close array
    return write_char(ctx, ']');
}

sjson_status_t sjson_AddFloatArrayToObject(sjson_context_t *ctx, const char *key,
                                           const float *values, size_t count)
{
    if (!ctx || !key || !values)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_object_state(ctx);
    if (status != SJSON_OK)
        return status;

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    // Write: "key":[
    char buffer[64];
    int written = snprintf(buffer, sizeof(buffer), "\"%s\":[", key);

    if (written < 0 || written >= (int)sizeof(buffer))
    {
        return SJSON_ERROR_BUFFER_FULL;
    }

    status = write_str(ctx, buffer);
    if (status != SJSON_OK)
        return status;

    // Add array elements
    for (size_t i = 0; i < count; i++)
    {
        char value_buffer[32];
        written = snprintf(value_buffer, sizeof(value_buffer), "%s%.6f",
                           i > 0 ? "," : "", values[i]);

        if (written < 0 || written >= (int)sizeof(value_buffer))
        {
            return SJSON_ERROR_BUFFER_FULL;
        }

        status = write_str(ctx, value_buffer);
        if (status != SJSON_OK)
            return status;
    }

    // Close array
    return write_char(ctx, ']');
}

sjson_status_t sjson_AddArrayToObject(sjson_context_t *ctx, const char *key)
{
    if (!ctx || !key ||strlen(key) == 0 || strlen(key) > 128)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_object_state(ctx);
    if (status != SJSON_OK)
        return status;

    if (ctx->depth >= ctx->max_depth)
    {
        return SJSON_ERROR_MAX_DEPTH;
    }

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    // Write: "<key>":[
    char buffer[128+5]; // extraspsaces for "":[ and null terminator
    int written = snprintf(buffer, sizeof(buffer), "\"%s\":[", key);

    if (written < 0 || written >= (int)sizeof(buffer))
    {
        return SJSON_ERROR_BUFFER_FULL;
    }

    status = write_str(ctx, buffer);
    if (status != SJSON_OK)
        return status;

    // Push array onto stack
    ctx->depth_stack[ctx->depth] = ']';
    ctx->depth++;
    ctx->needs_comma[ctx->depth] = false;

    return SJSON_OK;
}

sjson_status_t sjson_AddObjectToObject(sjson_context_t *ctx, const char *key)
{
    if (!ctx || !key)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_object_state(ctx);
    if (status != SJSON_OK)
        return status;

    if (ctx->depth >= ctx->max_depth)
    {
        return SJSON_ERROR_MAX_DEPTH;
    }

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    // Write: "key":{
    char buffer[128];
    int written = snprintf(buffer, sizeof(buffer), "\"%s\":{", key);

    if (written < 0 || written >= (int)sizeof(buffer))
    {
        return SJSON_ERROR_BUFFER_FULL;
    }

    status = write_str(ctx, buffer);
    if (status != SJSON_OK)
        return status;

    // Push object onto stack
    ctx->depth_stack[ctx->depth] = '}';
    ctx->depth++;
    ctx->needs_comma[ctx->depth] = false;

    return SJSON_OK;
}

sjson_status_t sjson_AddRawToObject(sjson_context_t *ctx, const char *key, const char *value)
{
    if (!ctx || !key || !value)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_object_state(ctx);
    if (status != SJSON_OK)
        return status;

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    // Write: "key":value (value is raw JSON)
    status = write_char(ctx, '"');
    if (status != SJSON_OK)
        return status;

    status = write_str(ctx, key);
    if (status != SJSON_OK)
        return status;

    status = write_str(ctx, "\":");
    if (status != SJSON_OK)
        return status;

    return write_str(ctx, value);
}

/* ========================================================================
 * Add to Array
 * ======================================================================== */

sjson_status_t sjson_AddIntToArray(sjson_context_t *ctx, int64_t value)
{
    if (!ctx)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    // Check we're in an array (not object)
    sjson_status_t status = check_array_state(ctx);
    if (status != SJSON_OK)
        return status;

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    char buffer[32];
    int written = snprintf(buffer, sizeof(buffer), "%ld", (long)value);

    if (written < 0 || written >= (int)sizeof(buffer))
    {
        return SJSON_ERROR_BUFFER_FULL;
    }

    return write_str(ctx, buffer);
}

sjson_status_t sjson_AddFloatToArray(sjson_context_t *ctx, float value)
{
    if (!ctx)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_array_state(ctx);
    if (status != SJSON_OK)
        return status;

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    char buffer[32];
    int written = snprintf(buffer, sizeof(buffer), "%.6f", value);

    if (written < 0 || written >= (int)sizeof(buffer))
    {
        return SJSON_ERROR_BUFFER_FULL;
    }

    return write_str(ctx, buffer);
}

sjson_status_t sjson_AddStringToArray(sjson_context_t *ctx, const char *value)
{
    if (!ctx || !value)
    {
        return SJSON_ERROR_INVALID_PARAM;
    }

    sjson_status_t status = check_array_state(ctx);
    if (status != SJSON_OK)
        return status;

    status = add_comma_if_needed(ctx);
    if (status != SJSON_OK)
        return status;

    status = write_char(ctx, '"');
    if (status != SJSON_OK)
        return status;

    status = write_str(ctx, value);
    if (status != SJSON_OK)
        return status;

    return write_char(ctx, '"');
}
