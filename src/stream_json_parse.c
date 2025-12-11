#include "stream_json_parse.h"

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"

const static char *TAG = "json_parse_toplevel";

static inline int is_whitespace(char c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static void shift_buffer_by_len(json_parse_state_t *parse_state, int shift_len)
{
    if (shift_len < parse_state->parse_buffer_len)
    {
        memmove(parse_state->parse_buffer, parse_state->parse_buffer + shift_len, parse_state->parse_buffer_len - shift_len);
    }
    parse_state->parse_buffer_len -= shift_len;                      // Reduce the length of the parse buffer
    parse_state->parse_buffer[parse_state->parse_buffer_len] = '\0'; // Null-terminate the remaining buffer

    reset_value_tracking_for_parse_state(parse_state); // Reset value tracking for the next key-value pair
}

void reset_value_tracking_for_parse_state(json_parse_state_t *p_state)
{
    if (!p_state)
        return;

    p_state->state = PARSE_STATE_SEEKING_KEY; // Reset state to seek next key
    p_state->key_pos = 0;                     // Reset key position
    p_state->bracket_depth = 0;               // Reset bracket depth
    p_state->brace_depth = 0;                 // Reset brace depth
    p_state->value_start = 0;
    p_state->value_len = 0;
    p_state->in_quotes = false;   // Reset in_quotes
    p_state->escape_next = false; // Reset escape next
}

void reset_json_parse_state(json_parse_state_t *p_state)
{
    if (!p_state)
        return;

    reset_value_tracking_for_parse_state(p_state); // Reset value tracking parameters
    p_state->parse_buffer_len = 0;                 // Reset the length of the parse buffer
    memset(p_state->key_buffer, 0, sizeof(p_state->key_buffer));
}

int check_remaining_buffer(json_parse_state_t *p_state)
{
    for (int i = 0; i < p_state->parse_buffer_len; i++)
    {
        if (!is_whitespace(p_state->parse_buffer[i]))
        {
            return 0;
        }
    }
    return 1;
}

int find_key_value_pair(json_parse_state_t *p_state)
{
    if (!p_state)
    {
        ESP_LOGE(TAG, "Invalid arguments to parse_json_response_chunk");
        return -1;
    }

    if (!p_state->state == PARSE_STATE_SEEKING_KEY)
    {
        ESP_LOGE(TAG, "Invalid initial state for JSON parsing: %d", p_state->state);
        return -1; // Invalid initial state
    }

    const char *current_data_ptr = p_state->parse_buffer;
    size_t current_data_len = p_state->parse_buffer_len;

    for (int i = 0; i < current_data_len; i++)
    {
        char c = current_data_ptr[i];

        if (p_state->escape_next)
        {
            p_state->escape_next = false;
            continue;
        }

        if (c == '\\' && p_state->in_quotes)
        {
            p_state->escape_next = true;
            continue;
        }

        switch (p_state->state)
        {
        case PARSE_STATE_SEEKING_KEY:
            if (c == '"')
            {
                p_state->state = PARSE_STATE_IN_KEY;
            }
            else if (c == '}')
            {
                // End of the object reached.
                shift_buffer_by_len(p_state, i + 1); // Shift the buffer to remove processed data
                if (!check_remaining_buffer(p_state))
                {
                    return -1;
                }
                return 0;
            }
            else if (!is_whitespace(c) && c != '{')
            { // for opening {. would be nicer to actually skip this once at beggining only
                ESP_LOGE(TAG, "Unexpected char '%c' (0x%02X) while seeking key. Buffer: %.*s", c, c, (int)current_data_len, current_data_ptr);
                reset_json_parse_state(p_state);
                return -1; // Invalid character while seeking key
            }
            break;

        case PARSE_STATE_IN_KEY:
            if (c == '"')
            {
                p_state->key_buffer[p_state->key_pos] = '\0';
                p_state->state = PARSE_STATE_SEEKING_COLON;
            }
            else if (p_state->key_pos < sizeof(p_state->key_buffer) - 1)
            {
                p_state->key_buffer[p_state->key_pos++] = c;
            }
            else
            {
                ESP_LOGE(TAG, "Key buffer overflow for key starting with: %s", p_state->key_buffer);
            }
            break;

        case PARSE_STATE_SEEKING_COLON:
            if (c == ':')
            {
                p_state->state = PARSE_STATE_SEEKING_VALUE;
            }
            else if (!is_whitespace(c))
            {
                ESP_LOGE(TAG, "Unexpected char '%c' (0x%02X) after key '%s', seeking colon. Buffer: %.*s", c, c, p_state->key_buffer, (int)current_data_len, current_data_ptr);
                reset_json_parse_state(p_state);
                return -1;
            }
            break;

        case PARSE_STATE_SEEKING_VALUE:
            if (!is_whitespace(c))
            {
                p_state->state = PARSE_STATE_IN_VALUE;
                p_state->value_start = i; // Relative to current_data_ptr (start of parse_buffer)

                switch (c)
                {
                case '"':
                    p_state->in_quotes = true; // Start of a string value
                    break;
                case '{':
                    p_state->brace_depth = 1; // Start of an object value
                    break;
                case '[':
                    p_state->bracket_depth = 1; // Start of an array value
                    break;
                default:
                    break;
                }
            }
            break;

        case PARSE_STATE_IN_VALUE:
            if (c == '"' && !p_state->escape_next)
            { // Not p_state->escape_next ensures this is not an escaped quote
                p_state->in_quotes = !p_state->in_quotes;
            }

            if (p_state->in_quotes)
            {
                continue;
            }

            // Value is complete if not in quotes, depths are balanced, and we hit a delimiter
            if (p_state->brace_depth == 0 && p_state->bracket_depth == 0 && (c == ',' || c == '}'))
            {

                p_state->value_len = i - p_state->value_start;
                return 1; // Found a complete key-value pair
            }

            if (c == '{')
                p_state->brace_depth++;
            else if (c == '}')
                p_state->brace_depth--;
            else if (c == '[')
                p_state->bracket_depth++;
            else if (c == ']')
                p_state->bracket_depth--;

            break;
        }

    } // end for loop

    // we got here, so we didn't find a complete key-value pair
    return 0;
}

void shift_parse_buffer_by_key_value(json_parse_state_t *parse_state)
{
    if (!parse_state || parse_state->parse_buffer_len == 0 || parse_state->value_len <= 0)
    {
        ESP_LOGE(TAG, "Invalid arguments to shift_parse_buffer_by_key_value");
        return;
    }

    int shift_len = parse_state->value_start + parse_state->value_len + 1; // +1 for the delimiter
    shift_buffer_by_len(parse_state, shift_len);
}
