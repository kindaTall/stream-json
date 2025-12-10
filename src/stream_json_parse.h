#pragma once

#include <stdbool.h>

/** @brief Enum for JSON parser state - Moved from heartbeat.h */
typedef enum
{
    PARSE_STATE_SEEKING_KEY,
    PARSE_STATE_IN_KEY,
    PARSE_STATE_SEEKING_COLON,
    PARSE_STATE_SEEKING_VALUE,
    PARSE_STATE_IN_VALUE
} parse_state_enum_t;

/** @brief Struct for JSON parser state - Moved from heartbeat.h */
typedef struct
{
    parse_state_enum_t state;
    char key_buffer[64];       // Buffer for the current key
    int key_pos;               // Current position in the key_buffer
    int bracket_depth;         // Tracks nesting of [] for the current value
    int brace_depth;           // Tracks nesting of {} for the current value
    bool in_quotes;            // True if parser is currently inside a string
    bool escape_next;          // True if the next character is escaped
    int value_start;           // Start index of the current value in the parse buffer
    int value_len;             // Length of the current value
    char *parse_buffer;        // Pointer to the buffer being parsed
    int parse_buffer_len;      // Length of the current parse buffer
    int parse_buffer_capacity; // Capacity of the parse buffer
    // Additional fields for robust parsing if needed, e.g., overall_brace_depth
} json_parse_state_t;

/**
 * @brief Resets the value tracking parameters in the JSON parse state.
 */
void reset_value_tracking_for_parse_state(json_parse_state_t *p_state);

/**
 * @brief Resets the JSON parse state to its initial values.
 * This function clears the key buffer, resets the state to seeking key,
 * and initializes other parsing parameters.
 */
void reset_json_parse_state(json_parse_state_t *p_state);

/**
 * @brief Checks if the remaining buffer contains only whitespace characters.
 * This function iterates through the parse buffer and returns 1 if all characters
 * are whitespace, otherwise returns 0.
 */
int check_remaining_buffer(json_parse_state_t *p_state);

/**
 * @brief Finds a complete key-value pair in the JSON parse buffer.
 *
 * This function scans the parse buffer for a complete key-value pair,
 * updating the json_parse_state_t structure with the found key and value.
 *
 * @return 1 if a complete key-value pair is found, 0 if not found,
 *         or -1 if an error occurs (e.g., invalid state, unexpected characters).
 */
int find_key_value_pair(json_parse_state_t *p_state);

/**
 * @brief Shifts the parse buffer by removing the processed key-value pair.
 */
void shift_parse_buffer_by_key_value(json_parse_state_t *parse_state);