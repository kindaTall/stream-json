/**
 * @file example_usage.c
 * @brief Example usage of stream_json library
 *
 * Demonstrates various use cases with printf-based callback.
 *
 * Compile: gcc example_usage.c ../src/stream_json_write.c -I../src -o example_usage
 * Run: ./example_usage
 */

#include <stdio.h>
#include <string.h>
#include "../src/stream_json.h"

/* Simple callback that prints to stdout */
bool print_callback(const char *buffer, size_t length, void *user_data)
{
    (void)user_data;
    printf("%.*s", (int)length, buffer);
    fflush(stdout);
    return true;
}

/* Example 1: Simple flat object */
void example_flat_object(void)
{
    char buffer[512];
    sjson_context_t ctx;

    printf("Example 1: Flat object\n");
    printf("=======================\n");

    sjson_InitObject(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    sjson_AddStringToObject(&ctx, "device", "ESP32");
    sjson_AddStringToObject(&ctx, "status", "online");
    sjson_AddIntToObject(&ctx, "uptime_sec", 3600);
    sjson_AddFloatToObject(&ctx, "temperature", 23.45);
    sjson_End(&ctx);

    printf("\n\n");
}

/* Example 2: Object with arrays */
void example_object_with_arrays(void)
{
    char buffer[512];
    sjson_context_t ctx;

    printf("Example 2: Object with arrays\n");
    printf("==============================\n");

    sjson_InitObject(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    sjson_AddStringToObject(&ctx, "sensor", "DHT22");

    // Convenience array functions
    float temps[] = {23.1, 23.2, 23.3, 23.4};
    sjson_AddFloatArrayToObject(&ctx, "temperatures", temps, 4);

    int64_t timestamps[] = {1000, 2000, 3000, 4000};
    sjson_AddIntArrayToObject(&ctx, "timestamps", timestamps, 4);

    sjson_End(&ctx);

    printf("\n\n");
}

/* Example 3: Manual array construction */
void example_manual_array(void)
{
    char buffer[512];
    sjson_context_t ctx;

    printf("Example 3: Manual array construction\n");
    printf("=====================================\n");

    sjson_InitObject(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    sjson_AddStringToObject(&ctx, "sensor", "NTC");

    // Open array and add elements one by one
    sjson_AddArrayToObject(&ctx, "readings");
    sjson_AddFloatToArray(&ctx, 23.1);
    sjson_AddFloatToArray(&ctx, 23.2);
    sjson_AddFloatToArray(&ctx, 23.3);
    sjson_Close(&ctx);  // Close array

    sjson_AddIntToObject(&ctx, "count", 3);
    sjson_End(&ctx);

    printf("\n\n");
}

/* Example 4: Nested object */
void example_nested_object(void)
{
    char buffer[512];
    sjson_context_t ctx;

    printf("Example 4: Nested object (1 level)\n");
    printf("===================================\n");

    sjson_InitObject(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    sjson_AddStringToObject(&ctx, "status", "ok");

    // Open nested object
    sjson_AddObjectToObject(&ctx, "metadata");
    sjson_AddStringToObject(&ctx, "version", "1.0");
    sjson_AddIntToObject(&ctx, "build", 42);
    sjson_AddStringToObject(&ctx, "author", "user");
    sjson_Close(&ctx);  // Close metadata object

    sjson_AddIntToObject(&ctx, "count", 100);
    sjson_End(&ctx);

    printf("\n\n");
}

/* Example 5: Root array instead of object */
void example_root_array(void)
{
    char buffer[512];
    sjson_context_t ctx;

    printf("Example 5: Root array\n");
    printf("=====================\n");

    sjson_InitArray(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    sjson_AddIntToArray(&ctx, 1);
    sjson_AddIntToArray(&ctx, 2);
    sjson_AddIntToArray(&ctx, 3);
    sjson_AddStringToArray(&ctx, "hello");
    sjson_AddFloatToArray(&ctx, 3.14);
    sjson_End(&ctx);

    printf("\n\n");
}

/* Example 6: Small buffer with auto-flush (streaming) */
void example_streaming(void)
{
    char buffer[64];  // Very small buffer to force multiple flushes
    sjson_context_t ctx;

    printf("Example 6: Small buffer (streaming demo)\n");
    printf("=========================================\n");
    printf("Buffer size: %zu bytes\n", sizeof(buffer));
    printf("Output: ");

    sjson_InitObject(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    sjson_AddStringToObject(&ctx, "message", "This is a longer message that will span multiple flushes");
    sjson_AddIntToObject(&ctx, "number", 123456789);
    sjson_AddStringToObject(&ctx, "another", "More data to demonstrate streaming behavior");
    sjson_End(&ctx);

    printf("\n\n");
}

/* Example 7: Manual flush */
void example_manual_flush(void)
{
    char buffer[512];
    sjson_context_t ctx;

    printf("Example 7: Manual flush control\n");
    printf("================================\n");

    sjson_InitObject(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    sjson_AddStringToObject(&ctx, "status", "processing");
    sjson_Flush(&ctx);  // Force immediate output
    printf(" <-- flushed immediately\n");

    // Simulate some work
    printf("(simulating work...)\n");

    sjson_AddIntToObject(&ctx, "progress", 50);
    sjson_Flush(&ctx);  // Another manual flush
    printf(" <-- flushed again\n");

    sjson_AddStringToObject(&ctx, "final", "done");
    sjson_End(&ctx);
    printf(" <-- final flush on End()\n\n");
}

/* Example 8: Error handling */
void example_error_handling(void)
{
    char buffer[512];
    sjson_context_t ctx;
    sjson_status_t status;

    printf("Example 8: Error handling\n");
    printf("=========================\n");

    status = sjson_InitObject(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    printf("Init: %s\n", status == SJSON_OK ? "OK" : "FAIL");

    status = sjson_AddStringToObject(&ctx, "test", "value");
    printf("AddString: %s\n", status == SJSON_OK ? "OK" : "FAIL");

    // Try to add to array (should fail - we're in an object)
    status = sjson_AddIntToArray(&ctx, 42);
    printf("AddIntToArray (should fail): %s\n", status == SJSON_OK ? "OK" : "FAIL");

    status = sjson_End(&ctx);
    printf("End: %s\n", status == SJSON_OK ? "OK" : "FAIL");

    // Try to use after finalized (should fail)
    status = sjson_AddStringToObject(&ctx, "after", "finalized");
    printf("AddString after End (should fail): %s\n\n", status == SJSON_OK ? "OK" : "FAIL");

    printf("JSON output: ");
    sjson_InitObject(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    sjson_AddStringToObject(&ctx, "test", "value");
    sjson_End(&ctx);
    printf("\n\n");
}

/* Example 9: Raw JSON insertion */
void example_raw_json(void)
{
    char buffer[512];
    sjson_context_t ctx;

    printf("Example 9: Raw JSON insertion\n");
    printf("==============================\n");

    sjson_InitObject(&ctx, buffer, sizeof(buffer), print_callback, NULL);
    sjson_AddStringToObject(&ctx, "status", "ok");

    // Insert pre-serialized JSON (e.g., from another source)
    sjson_AddRawToObject(&ctx, "nested", "{\"x\":1,\"y\":2}");

    sjson_AddIntToObject(&ctx, "count", 42);
    sjson_End(&ctx);

    printf("\n\n");
}

int main(void)
{
    printf("========================================\n");
    printf("stream_json Library Examples\n");
    printf("========================================\n\n");

    example_flat_object();
    example_object_with_arrays();
    example_manual_array();
    example_nested_object();
    example_root_array();
    example_streaming();
    example_manual_flush();
    example_error_handling();
    example_raw_json();

    printf("========================================\n");
    printf("All examples completed successfully!\n");
    printf("========================================\n");

    return 0;
}
