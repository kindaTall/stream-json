# stream_json

Zero-malloc streaming JSON generator for embedded systems.

## Features

- **Zero dynamic allocation**: All memory from caller-provided fixed buffer
- **Streaming**: Automatic flush via callback when buffer fills
- **Bounded memory**: Predictable, constant memory usage
- **Portable**: Pure C99, no platform dependencies
- **cJSON-compatible API**: Familiar naming conventions
- **Nested structures**: Support for objects and arrays up to 8 levels deep

## Why This Library?

Traditional JSON libraries like cJSON allocate memory in two places:
1. JSON tree structure (64 bytes per node)
2. Final string output (unbounded allocation)

For 360 temperature readings, cJSON needs:
- Tree: 1,801 nodes × 64 bytes = 112 KB
- String: ~46 KB
- **Peak usage: ~158 KB** (both allocated at once)

With stream_json:
- Buffer: 512 bytes (fixed)
- Streams chunks via callback
- **Total usage: 512 bytes**

## Quick Start

```c
#include "stream_json.h"

// Callback that sends JSON chunks (e.g., over HTTP, UART, etc.)
bool send_callback(const char *buffer, size_t length, void *user_data) {
    // Send the data somewhere
    fwrite(buffer, 1, length, stdout);
    return true;  // Return false on error
}

int main(void) {
    char buffer[512];
    sjson_context_t ctx;

    // Initialize with root object
    sjson_InitObject(&ctx, buffer, sizeof(buffer), send_callback, NULL);

    // Add key-value pairs
    sjson_AddStringToObject(&ctx, "device", "ESP32");
    sjson_AddIntToObject(&ctx, "uptime", 3600);
    sjson_AddFloatToObject(&ctx, "temperature", 23.5);

    // Finalize and flush
    sjson_End(&ctx);

    return 0;
}
```

Output: `{"device":"ESP32","uptime":3600,"temperature":23.500000}`

## API Reference

### Initialization

#### `sjson_InitObject()`
Start JSON with root object `{}`
```c
sjson_status_t sjson_InitObject(sjson_context_t *ctx, char *buffer,
                                size_t buffer_size,
                                sjson_send_callback_t callback,
                                void *user_data);
```

#### `sjson_InitArray()`
Start JSON with root array `[]`
```c
sjson_status_t sjson_InitArray(sjson_context_t *ctx, char *buffer,
                               size_t buffer_size,
                               sjson_send_callback_t callback,
                               void *user_data);
```

**Parameters:**
- `ctx`: Context to initialize
- `buffer`: Pre-allocated buffer (recommended: 512-2048 bytes)
- `buffer_size`: Size of buffer in bytes
- `callback`: Function called when buffer fills or on finalization
- `user_data`: Optional pointer passed to callback

### Adding to Objects

```c
sjson_AddStringToObject(&ctx, "key", "value");
sjson_AddIntToObject(&ctx, "key", 42);
sjson_AddFloatToObject(&ctx, "key", 3.14f);
sjson_AddNumberToObject(&ctx, "key", 2.71828);  // double precision
```

#### Nested Collections
```c
// Add nested array
sjson_AddArrayToObject(&ctx, "items");
sjson_AddIntToArray(&ctx, 1);
sjson_AddIntToArray(&ctx, 2);
sjson_Close(&ctx);  // Close the array

// Add nested object
sjson_AddObjectToObject(&ctx, "metadata");
sjson_AddStringToObject(&ctx, "version", "1.0");
sjson_Close(&ctx);  // Close the object
```

#### Convenience Functions
```c
// Add entire array at once
int64_t values[] = {1, 2, 3, 4};
sjson_AddIntArrayToObject(&ctx, "timestamps", values, 4);

float temps[] = {23.1, 23.2, 23.3};
sjson_AddFloatArrayToObject(&ctx, "temperatures", temps, 3);
```

#### Raw JSON
```c
// Insert pre-serialized JSON (not escaped)
sjson_AddRawToObject(&ctx, "config", "{\"x\":1,\"y\":2}");
```

### Adding to Arrays

```c
sjson_AddIntToArray(&ctx, 42);
sjson_AddFloatToArray(&ctx, 3.14f);
sjson_AddStringToArray(&ctx, "hello");
```

### Finalization

#### `sjson_Close()`
Close current collection (array or object). Automatically writes `}` or `]` based on context.
```c
sjson_status_t sjson_Close(sjson_context_t *ctx);
```

#### `sjson_End()`
Close all open collections and flush remaining data. Marks context as finalized.
```c
sjson_status_t sjson_End(sjson_context_t *ctx);
```

#### `sjson_Flush()`
Manually flush buffer without closing collections.
```c
sjson_status_t sjson_Flush(sjson_context_t *ctx);
```

### Status Codes

```c
typedef enum {
    SJSON_OK = 0,                  // Success
    SJSON_ERROR_INVALID_STATE,     // Invalid operation for current state
    SJSON_ERROR_MAX_DEPTH,         // Max nesting depth (8) reached
    SJSON_ERROR_BUFFER_FULL,       // Buffer full and callback failed
    SJSON_ERROR_INVALID_PARAM      // NULL pointer or invalid parameter
} sjson_status_t;
```

## Usage Examples

### Nested Objects and Arrays

```c
sjson_InitObject(&ctx, buffer, sizeof(buffer), send_callback, NULL);

sjson_AddStringToObject(&ctx, "status", "ok");

// Nested object
sjson_AddObjectToObject(&ctx, "metadata");
sjson_AddStringToObject(&ctx, "version", "1.0");
sjson_AddIntToObject(&ctx, "build", 42);
sjson_Close(&ctx);  // Close metadata

// Nested array
sjson_AddArrayToObject(&ctx, "readings");
sjson_AddFloatToArray(&ctx, 23.1);
sjson_AddFloatToArray(&ctx, 23.2);
sjson_Close(&ctx);  // Close readings

sjson_End(&ctx);
```

Output:
```json
{"status":"ok","metadata":{"version":"1.0","build":42},"readings":[23.1,23.2]}
```

### Root Array

```c
sjson_InitArray(&ctx, buffer, sizeof(buffer), send_callback, NULL);
sjson_AddIntToArray(&ctx, 1);
sjson_AddIntToArray(&ctx, 2);
sjson_AddStringToArray(&ctx, "hello");
sjson_End(&ctx);
```

Output: `[1,2,"hello"]`

### Streaming with Small Buffer

```c
char buffer[64];  // Small buffer forces multiple flushes
sjson_InitObject(&ctx, buffer, sizeof(buffer), send_callback, NULL);
sjson_AddStringToObject(&ctx, "message", "This is a very long message...");
// Callback automatically invoked when buffer fills
sjson_End(&ctx);
```

### Manual Flush Control

```c
sjson_InitObject(&ctx, buffer, sizeof(buffer), send_callback, NULL);
sjson_AddStringToObject(&ctx, "status", "processing");
sjson_Flush(&ctx);  // Send immediately

// ... do some work ...

sjson_AddIntToObject(&ctx, "progress", 50);
sjson_End(&ctx);
```

## Building

### CMake
```bash
mkdir build && cd build
cmake ..
make
./bin/write_examples
```

### Manual Compilation
```bash
gcc your_app.c src/stream_json_write.c -Isrc -o your_app
```

### ESP-IDF Component

```bash
# In your ESP-IDF project
mkdir -p components/stream_json
cp src/* components/stream_json/

# Create components/stream_json/CMakeLists.txt:
idf_component_register(SRCS "stream_json_write.c"
                       INCLUDE_DIRS ".")
```

## Integration

The library is designed to be portable - just copy the two files:
- `src/stream_json.h`
- `src/stream_json_write.c`

No build system required. Pure C99 with no dependencies.

## Limitations

- Maximum nesting depth: 8 levels (configurable via `SJSON_MAX_DEPTH`)
- No pretty-printing (compact JSON only)
- Float formatting uses `%f` (6 decimal places by default)
- Strings are escaped, but unicode handling is basic

## Thread Safety

Not thread-safe. Each thread should use its own `sjson_context_t` instance.

## Examples

See `examples/write_examples.c` for comprehensive usage examples including:
- Flat objects
- Arrays (convenience and manual)
- Nested objects and arrays
- Streaming with small buffers
- Error handling
- Manual flush control
- Raw JSON insertion

## License

MIT License - see LICENSE file for details.
