#include "text_input.h"
#include "shadow.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// Helper function to get effective style values
static ComponentStyle* GetEffectiveStyle(TextInput* input) {
    return Theme_GetComponentStyle(COMPONENT_TEXT_INPUT);
}

static ComponentState GetCurrentState(TextInput* input) {
    if (input->is_disabled) return STATE_DISABLED;
    if (input->is_focused) return STATE_ACTIVE;
    if (input->is_hovered) return STATE_HOVER;
    return STATE_DEFAULT;
}

// Helper function to calculate text position for cursor/selection
static int GetCharacterIndexAtPosition(TextInput* input, float x_pos) {
    if (!input->text) return 0;
    
    ComponentStyle* style = GetEffectiveStyle(input);
    int font_size = input->custom_font_size ? *input->custom_font_size : style->font_size;
    EdgeValues padding = input->custom_padding ? *input->custom_padding : style->padding;
    
    float text_start_x = input->bounds.x + padding.left - input->scroll_offset;
    float relative_x = x_pos - text_start_x;
    
    if (relative_x <= 0) return 0;
    
    int text_len = strlen(input->text);
    for (int i = 0; i <= text_len; i++) {
        char temp_char = input->text[i];
        input->text[i] = '\0';
        
        Vector2 text_size = MeasureTextEx(GetFontDefault(), input->text, font_size, 1.0f);
        input->text[i] = temp_char;
        
        if (text_size.x >= relative_x) {
            return i;
        }
    }
    
    return text_len;
}

static float GetCursorXPosition(TextInput* input) {
    if (!input->text) return input->bounds.x;
    
    ComponentStyle* style = GetEffectiveStyle(input);
    int font_size = input->custom_font_size ? *input->custom_font_size : style->font_size;
    EdgeValues padding = input->custom_padding ? *input->custom_padding : style->padding;
    
    char temp_char = input->text[input->cursor_position];
    input->text[input->cursor_position] = '\0';
    
    Vector2 text_size = MeasureTextEx(GetFontDefault(), input->text, font_size, 1.0f);
    input->text[input->cursor_position] = temp_char;
    
    return input->bounds.x + padding.left + text_size.x - input->scroll_offset;
}

static void UpdateScrollOffset(TextInput* input) {
    ComponentStyle* style = GetEffectiveStyle(input);
    EdgeValues padding = input->custom_padding ? *input->custom_padding : style->padding;
    
    float cursor_x = GetCursorXPosition(input);
    float input_left = input->bounds.x + padding.left;
    float input_right = input->bounds.x + input->bounds.width - padding.right;
    
    // Scroll right if cursor is beyond right edge
    if (cursor_x > input_right) {
        input->scroll_offset += cursor_x - input_right + 10;
    }
    // Scroll left if cursor is beyond left edge
    else if (cursor_x < input_left) {
        input->scroll_offset -= input_left - cursor_x + 10;
        if (input->scroll_offset < 0) input->scroll_offset = 0;
    }
}

static void InsertCharacter(TextInput* input, char c) {
    if (!input->text || input->is_readonly || input->is_disabled) return;
    
    int text_len = strlen(input->text);
    if (text_len >= input->max_length - 1) return;
    
    // Delete selection if any
    if (TextInput_HasSelection(input)) {
        int start = input->selection_start < input->selection_end ? input->selection_start : input->selection_end;
        int end = input->selection_start > input->selection_end ? input->selection_start : input->selection_end;
        
        memmove(input->text + start, input->text + end, text_len - end + 1);
        input->cursor_position = start;
        input->selection_start = input->selection_end = input->cursor_position;
        text_len = strlen(input->text);
    }
    
    // Insert character
    memmove(input->text + input->cursor_position + 1, 
            input->text + input->cursor_position, 
            text_len - input->cursor_position + 1);
    input->text[input->cursor_position] = c;
    input->cursor_position++;
    
    // Validate and trigger callback
    if (input->validator) {
        input->is_valid = input->validator(input->text);
    }
    
    if (input->on_text_changed) {
        input->on_text_changed(input->text, input->user_data);
    }
    
    UpdateScrollOffset(input);
}

static void DeleteCharacter(TextInput* input, bool forward) {
    if (!input->text || input->is_readonly || input->is_disabled) return;
    
    int text_len = strlen(input->text);
    
    // Delete selection if any
    if (TextInput_HasSelection(input)) {
        int start = input->selection_start < input->selection_end ? input->selection_start : input->selection_end;
        int end = input->selection_start > input->selection_end ? input->selection_start : input->selection_end;
        
        memmove(input->text + start, input->text + end, text_len - end + 1);
        input->cursor_position = start;
        input->selection_start = input->selection_end = input->cursor_position;
    } else {
        // Delete single character
        if (forward) {
            if (input->cursor_position < text_len) {
                memmove(input->text + input->cursor_position, 
                        input->text + input->cursor_position + 1, 
                        text_len - input->cursor_position);
            }
        } else {
            if (input->cursor_position > 0) {
                memmove(input->text + input->cursor_position - 1, 
                        input->text + input->cursor_position, 
                        text_len - input->cursor_position + 1);
                input->cursor_position--;
            }
        }
    }
    
    // Validate and trigger callback
    if (input->validator) {
        input->is_valid = input->validator(input->text);
    }
    
    if (input->on_text_changed) {
        input->on_text_changed(input->text, input->user_data);
    }
    
    UpdateScrollOffset(input);
}

TextInput TextInput_Create(Rectangle bounds, const char* placeholder) {
    return TextInput_CreateWithType(bounds, placeholder, INPUT_TYPE_TEXT);
}

TextInput TextInput_CreateWithType(Rectangle bounds, const char* placeholder, TextInputType type) {
    TextInput input = {0};
    
    input.bounds = bounds;
    input.type = type;
    input.max_length = AppTheme.text_input_props.max_length;
    
    // Allocate text buffer
    input.text = (char*)calloc(input.max_length, sizeof(char));
    
    // Set placeholder
    if (placeholder) {
        size_t placeholder_len = strlen(placeholder) + 1;
        input.placeholder = (char*)malloc(placeholder_len);
        strcpy(input.placeholder, placeholder);
    }
    
    // Initialize state
    input.is_focused = false;
    input.is_hovered = false;
    input.is_disabled = false;
    input.is_readonly = false;
    input.is_visible = true;
    
    // Initialize cursor and selection
    input.cursor_position = 0;
    input.selection_start = 0;
    input.selection_end = 0;
    input.cursor_blink_timer = 0.0f;
    input.cursor_visible = true;
    
    // Initialize scrolling
    input.scroll_offset = 0.0f;
    input.text_width = 0.0f;
    
    // Initialize validation
    input.validator = NULL;
    input.is_valid = true;
    input.error_message = NULL;
    
    // Initialize visual settings
    input.show_border = true;
    input.show_shadow = false;
    input.show_placeholder = AppTheme.text_input_props.show_placeholder;
    
    // Initialize custom styling pointers to NULL
    input.custom_background_color = NULL;
    input.custom_text_color = NULL;
    input.custom_border_color = NULL;
    input.custom_cursor_color = NULL;
    input.custom_selection_color = NULL;
    input.custom_placeholder_color = NULL;
    input.custom_padding = NULL;
    input.custom_font_size = NULL;
    
    // Initialize callbacks
    input.on_text_changed = NULL;
    input.on_enter_pressed = NULL;
    input.on_focus_gained = NULL;
    input.on_focus_lost = NULL;
    input.user_data = NULL;
    
    return input;
}

void TextInput_Destroy(TextInput* input) {
    if (!input) return;
    
    if (input->text) {
        free(input->text);
        input->text = NULL;
    }
    
    if (input->placeholder) {
        free(input->placeholder);
        input->placeholder = NULL;
    }
    
    if (input->error_message) {
        free(input->error_message);
        input->error_message = NULL;
    }
    
    // Free custom styling
    if (input->custom_background_color) { free(input->custom_background_color); input->custom_background_color = NULL; }
    if (input->custom_text_color) { free(input->custom_text_color); input->custom_text_color = NULL; }
    if (input->custom_border_color) { free(input->custom_border_color); input->custom_border_color = NULL; }
    if (input->custom_cursor_color) { free(input->custom_cursor_color); input->custom_cursor_color = NULL; }
    if (input->custom_selection_color) { free(input->custom_selection_color); input->custom_selection_color = NULL; }
    if (input->custom_placeholder_color) { free(input->custom_placeholder_color); input->custom_placeholder_color = NULL; }
    if (input->custom_padding) { free(input->custom_padding); input->custom_padding = NULL; }
    if (input->custom_font_size) { free(input->custom_font_size); input->custom_font_size = NULL; }
}

void TextInput_Update(TextInput* input) {
    if (!input || !input->is_visible) return;
    
    Vector2 mouse_pos = GetMousePosition();
    bool mouse_over = CheckCollisionPointRec(mouse_pos, input->bounds);
    
    input->is_hovered = mouse_over && !input->is_disabled;
    
    // Handle focus
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        bool was_focused = input->is_focused;
        input->is_focused = mouse_over && !input->is_disabled;
        
        if (input->is_focused && !was_focused) {
            // Gained focus
            if (AppTheme.text_input_props.auto_select_on_focus && input->text && strlen(input->text) > 0) {
                TextInput_SelectAll(input);
            }
            if (input->on_focus_gained) {
                input->on_focus_gained(input->user_data);
            }
        } else if (!input->is_focused && was_focused) {
            // Lost focus
            input->selection_start = input->selection_end = input->cursor_position;
            if (input->on_focus_lost) {
                input->on_focus_lost(input->user_data);
            }
        }
        
        // Set cursor position on click
        if (input->is_focused && mouse_over) {
            input->cursor_position = GetCharacterIndexAtPosition(input, mouse_pos.x);
            if (!IsKeyDown(KEY_LEFT_SHIFT)) {
                input->selection_start = input->selection_end = input->cursor_position;
            }
        }
    }
    
    if (!input->is_focused || input->is_disabled || input->is_readonly) {
        return;
    }
    
    // Handle text input
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126) { // Printable ASCII characters
            // Type-specific filtering
            bool allow_char = true;
            switch (input->type) {
                case INPUT_TYPE_NUMBER:
                    allow_char = isdigit(key) || key == '.' || key == '-' || key == '+';
                    break;
                case INPUT_TYPE_EMAIL:
                    allow_char = isalnum(key) || key == '@' || key == '.' || key == '_' || key == '-';
                    break;
                default:
                    break;
            }
            
            if (allow_char) {
                InsertCharacter(input, (char)key);
            }
        }
        key = GetCharPressed();
    }
    
    // Handle special keys
    if (IsKeyPressed(KEY_BACKSPACE)) {
        DeleteCharacter(input, false);
    }
    
    if (IsKeyPressed(KEY_DELETE)) {
        DeleteCharacter(input, true);
    }
    
    if (IsKeyPressed(KEY_ENTER)) {
        if (input->on_enter_pressed) {
            input->on_enter_pressed(input->text, input->user_data);
        }
    }
    
    // Handle cursor movement
    if (IsKeyPressed(KEY_LEFT)) {
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            if (input->selection_start == input->selection_end) {
                input->selection_start = input->cursor_position;
            }
            if (input->cursor_position > 0) input->cursor_position--;
            input->selection_end = input->cursor_position;
        } else {
            if (TextInput_HasSelection(input)) {
                input->cursor_position = input->selection_start < input->selection_end ? 
                                       input->selection_start : input->selection_end;
            } else if (input->cursor_position > 0) {
                input->cursor_position--;
            }
            input->selection_start = input->selection_end = input->cursor_position;
        }
        UpdateScrollOffset(input);
    }
    
    if (IsKeyPressed(KEY_RIGHT)) {
        int text_len = input->text ? strlen(input->text) : 0;
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            if (input->selection_start == input->selection_end) {
                input->selection_start = input->cursor_position;
            }
            if (input->cursor_position < text_len) input->cursor_position++;
            input->selection_end = input->cursor_position;
        } else {
            if (TextInput_HasSelection(input)) {
                input->cursor_position = input->selection_start > input->selection_end ? 
                                       input->selection_start : input->selection_end;
            } else if (input->cursor_position < text_len) {
                input->cursor_position++;
            }
            input->selection_start = input->selection_end = input->cursor_position;
        }
        UpdateScrollOffset(input);
    }
    
    if (IsKeyPressed(KEY_HOME)) {
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            if (input->selection_start == input->selection_end) {
                input->selection_start = input->cursor_position;
            }
            input->cursor_position = 0;
            input->selection_end = input->cursor_position;
        } else {
            input->cursor_position = 0;
            input->selection_start = input->selection_end = input->cursor_position;
        }
        UpdateScrollOffset(input);
    }
    
    if (IsKeyPressed(KEY_END)) {
        int text_len = input->text ? strlen(input->text) : 0;
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            if (input->selection_start == input->selection_end) {
                input->selection_start = input->cursor_position;
            }
            input->cursor_position = text_len;
            input->selection_end = input->cursor_position;
        } else {
            input->cursor_position = text_len;
            input->selection_start = input->selection_end = input->cursor_position;
        }
        UpdateScrollOffset(input);
    }
    
    // Handle Ctrl+A (Select All)
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) {
        TextInput_SelectAll(input);
    }
    
    // Handle Ctrl+C (Copy)
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_C)) {
        if (TextInput_HasSelection(input)) {
            char* selected = TextInput_GetSelectedText(input);
            if (selected) {
                SetClipboardText(selected);
                free(selected);
            }
        }
    }
    
    // Handle Ctrl+V (Paste)
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (clipboard) {
            // Insert clipboard text character by character
            for (int i = 0; clipboard[i] != '\0'; i++) {
                if (clipboard[i] >= 32 && clipboard[i] <= 126) {
                    InsertCharacter(input, clipboard[i]);
                }
            }
        }
    }
    
    // Update cursor blink
    input->cursor_blink_timer += GetFrameTime();
    if (input->cursor_blink_timer >= 1.0f / AppTheme.text_input_props.cursor_blink_speed) {
        input->cursor_visible = !input->cursor_visible;
        input->cursor_blink_timer = 0.0f;
    }
}

void TextInput_Draw(TextInput* input) {
    if (!input || !input->is_visible) return;
    
    ComponentStyle* style = GetEffectiveStyle(input);
    ComponentState state = GetCurrentState(input);
    ColorPalette* colors = &style->colors[state];
    
    // Get effective values
    EdgeValues padding = input->custom_padding ? *input->custom_padding : style->padding;
    int font_size = input->custom_font_size ? *input->custom_font_size : style->font_size;
    
    Color bg_color = input->custom_background_color ? *input->custom_background_color : colors->background;
    Color text_color = input->custom_text_color ? *input->custom_text_color : colors->text;
    Color border_color = input->custom_border_color ? *input->custom_border_color : colors->border;
    Color cursor_color = input->custom_cursor_color ? *input->custom_cursor_color : colors->accent;
    Color selection_color = input->custom_selection_color ? *input->custom_selection_color : colors->accent;
    selection_color.a = (unsigned char)(selection_color.a * AppTheme.text_input_props.selection_opacity);
    
    // Draw shadow
    if (input->show_shadow) {
        ShadowConfig shadow_config = Shadow_FromTheme(style, state);
        Shadow_DrawRectangleRounded(input->bounds, style->border_radius.top_left, &shadow_config);
    }
    
    // Draw background
    DrawRectangleRounded(input->bounds, style->border_radius.top_left / input->bounds.height, 8, bg_color);
    
    // Draw border
    if (input->show_border) {
        DrawRectangleRoundedLines(input->bounds, style->border_radius.top_left / input->bounds.height, 
                                 8, border_color);
    }
    
    // Calculate text area
    Rectangle text_area = {
        input->bounds.x + padding.left,
        input->bounds.y + padding.top,
        input->bounds.width - padding.left - padding.right,
        input->bounds.height - padding.top - padding.bottom
    };
    
    // Begin scissor mode for text clipping
    BeginScissorMode((int)text_area.x, (int)text_area.y, (int)text_area.width, (int)text_area.height);
    
    // Draw selection
    if (input->is_focused && TextInput_HasSelection(input)) {
        int start = input->selection_start < input->selection_end ? input->selection_start : input->selection_end;
        int end = input->selection_start > input->selection_end ? input->selection_start : input->selection_end;
        
        // Calculate selection bounds
        char temp_char1 = input->text[start];
        char temp_char2 = input->text[end];
        
        input->text[start] = '\0';
        Vector2 start_size = MeasureTextEx(GetFontDefault(), input->text, font_size, 1.0f);
        input->text[start] = temp_char1;
        
        input->text[end] = '\0';
        Vector2 end_size = MeasureTextEx(GetFontDefault(), input->text, font_size, 1.0f);
        input->text[end] = temp_char2;
        
        float selection_x = text_area.x + start_size.x - input->scroll_offset;
        float selection_width = end_size.x - start_size.x;
        
        DrawRectangle((int)selection_x, (int)text_area.y, (int)selection_width, (int)text_area.height, selection_color);
    }
    
    // Draw text or placeholder
    Vector2 text_pos = {text_area.x - input->scroll_offset, text_area.y + (text_area.height - font_size) * 0.5f};
    
    if (input->text && strlen(input->text) > 0) {
        // Draw actual text
        const char* display_text = input->text;
        char* password_text = NULL;
        
        if (input->type == INPUT_TYPE_PASSWORD) {
            int text_len = strlen(input->text);
            password_text = (char*)malloc(text_len + 1);
            for (int i = 0; i < text_len; i++) {
                password_text[i] = '*';
            }
            password_text[text_len] = '\0';
            display_text = password_text;
        }
        
        DrawTextEx(GetFontDefault(), display_text, text_pos, font_size, 1.0f, text_color);
        
        if (password_text) {
            free(password_text);
        }
    } else if (input->show_placeholder && input->placeholder && !input->is_focused) {
        // Draw placeholder text
        Color placeholder_color = input->custom_placeholder_color ? 
                                 *input->custom_placeholder_color : 
                                 (Color){text_color.r, text_color.g, text_color.b, (unsigned char)(text_color.a * 0.5f)};
        
        DrawTextEx(GetFontDefault(), input->placeholder, text_pos, font_size, 1.0f, placeholder_color);
    }
    
    // Draw cursor
    if (input->is_focused && input->cursor_visible && !input->is_disabled) {
        float cursor_x = GetCursorXPosition(input);
        float cursor_width = AppTheme.text_input_props.cursor_width;
        
        DrawRectangle((int)cursor_x, (int)text_area.y, (int)cursor_width, (int)text_area.height, cursor_color);
    }
    
    EndScissorMode();
    
    // Draw error message if invalid
    if (!input->is_valid && input->error_message) {
        Vector2 error_pos = {input->bounds.x, input->bounds.y + input->bounds.height + 5};
        DrawTextEx(GetFontDefault(), input->error_message, error_pos, font_size - 2, 1.0f, RED);
    }
}
// Text management functions
void TextInput_SetText(TextInput* input, const char* text) {
    if (!input || !input->text) return;
    
    if (text) {
        strncpy(input->text, text, input->max_length - 1);
        input->text[input->max_length - 1] = '\0';
    } else {
        input->text[0] = '\0';
    }
    
    // Reset cursor and selection
    int text_len = strlen(input->text);
    input->cursor_position = text_len;
    input->selection_start = input->selection_end = input->cursor_position;
    
    // Validate
    if (input->validator) {
        input->is_valid = input->validator(input->text);
    }
    
    // Trigger callback
    if (input->on_text_changed) {
        input->on_text_changed(input->text, input->user_data);
    }
    
    UpdateScrollOffset(input);
}

const char* TextInput_GetText(TextInput* input) {
    return input ? input->text : NULL;
}

void TextInput_Clear(TextInput* input) {
    TextInput_SetText(input, "");
}

void TextInput_SetPlaceholder(TextInput* input, const char* placeholder) {
    if (!input) return;
    
    if (input->placeholder) {
        free(input->placeholder);
        input->placeholder = NULL;
    }
    
    if (placeholder) {
        size_t placeholder_len = strlen(placeholder) + 1;
        input->placeholder = (char*)malloc(placeholder_len);
        strcpy(input->placeholder, placeholder);
    }
}

// State management functions
void TextInput_SetFocus(TextInput* input, bool focused) {
    if (!input || input->is_disabled) return;
    
    bool was_focused = input->is_focused;
    input->is_focused = focused;
    
    if (focused && !was_focused) {
        if (AppTheme.text_input_props.auto_select_on_focus && input->text && strlen(input->text) > 0) {
            TextInput_SelectAll(input);
        }
        if (input->on_focus_gained) {
            input->on_focus_gained(input->user_data);
        }
    } else if (!focused && was_focused) {
        input->selection_start = input->selection_end = input->cursor_position;
        if (input->on_focus_lost) {
            input->on_focus_lost(input->user_data);
        }
    }
}

bool TextInput_IsFocused(TextInput* input) {
    return input ? input->is_focused : false;
}

void TextInput_SetDisabled(TextInput* input, bool disabled) {
    if (!input) return;
    
    input->is_disabled = disabled;
    if (disabled) {
        input->is_focused = false;
        input->is_hovered = false;
    }
}

void TextInput_SetReadonly(TextInput* input, bool readonly) {
    if (input) input->is_readonly = readonly;
}

void TextInput_SetVisible(TextInput* input, bool visible) {
    if (input) input->is_visible = visible;
}

// Selection and cursor functions
void TextInput_SelectAll(TextInput* input) {
    if (!input || !input->text) return;
    
    input->selection_start = 0;
    input->selection_end = strlen(input->text);
    input->cursor_position = input->selection_end;
}

void TextInput_ClearSelection(TextInput* input) {
    if (!input) return;
    
    input->selection_start = input->selection_end = input->cursor_position;
}

void TextInput_SetCursorPosition(TextInput* input, int position) {
    if (!input || !input->text) return;
    
    int text_len = strlen(input->text);
    input->cursor_position = position < 0 ? 0 : (position > text_len ? text_len : position);
    input->selection_start = input->selection_end = input->cursor_position;
    
    UpdateScrollOffset(input);
}

int TextInput_GetCursorPosition(TextInput* input) {
    return input ? input->cursor_position : 0;
}

bool TextInput_HasSelection(TextInput* input) {
    return input && input->selection_start != input->selection_end;
}

char* TextInput_GetSelectedText(TextInput* input) {
    if (!input || !TextInput_HasSelection(input)) return NULL;
    
    int start = input->selection_start < input->selection_end ? input->selection_start : input->selection_end;
    int end = input->selection_start > input->selection_end ? input->selection_start : input->selection_end;
    int length = end - start;
    
    char* selected = (char*)malloc(length + 1);
    strncpy(selected, input->text + start, length);
    selected[length] = '\0';
    
    return selected;
}

// Configuration functions
void TextInput_SetMaxLength(TextInput* input, int max_length) {
    if (!input || max_length <= 0) return;
    
    // Reallocate text buffer if needed
    if (max_length != input->max_length) {
        char* new_text = (char*)realloc(input->text, max_length);
        if (new_text) {
            input->text = new_text;
            input->max_length = max_length;
            
            // Truncate text if it's now too long
            int current_len = strlen(input->text);
            if (current_len >= max_length) {
                input->text[max_length - 1] = '\0';
                input->cursor_position = strlen(input->text);
                input->selection_start = input->selection_end = input->cursor_position;
            }
        }
    }
}

void TextInput_SetType(TextInput* input, TextInputType type) {
    if (input) input->type = type;
}

void TextInput_SetValidator(TextInput* input, bool (*validator)(const char*)) {
    if (!input) return;
    
    input->validator = validator;
    if (validator && input->text) {
        input->is_valid = validator(input->text);
    }
}

bool TextInput_IsValid(TextInput* input) {
    return input ? input->is_valid : false;
}

void TextInput_SetErrorMessage(TextInput* input, const char* message) {
    if (!input) return;
    
    if (input->error_message) {
        free(input->error_message);
        input->error_message = NULL;
    }
    
    if (message) {
        size_t message_len = strlen(message) + 1;
        input->error_message = (char*)malloc(message_len);
        strcpy(input->error_message, message);
    }
}

// Visual customization functions
void TextInput_SetBorder(TextInput* input, bool show_border) {
    if (input) input->show_border = show_border;
}

void TextInput_SetShadow(TextInput* input, bool show_shadow) {
    if (input) input->show_shadow = show_shadow;
}

void TextInput_SetShowPlaceholder(TextInput* input, bool show) {
    if (input) input->show_placeholder = show;
}

// Styling functions
void TextInput_SetBackgroundColor(TextInput* input, Color color) {
    if (!input) return;
    
    if (!input->custom_background_color) {
        input->custom_background_color = (Color*)malloc(sizeof(Color));
    }
    
    if (input->custom_background_color) {
        *input->custom_background_color = color;
    }
}

void TextInput_SetTextColor(TextInput* input, Color color) {
    if (!input) return;
    
    if (!input->custom_text_color) {
        input->custom_text_color = (Color*)malloc(sizeof(Color));
    }
    
    if (input->custom_text_color) {
        *input->custom_text_color = color;
    }
}

void TextInput_SetBorderColor(TextInput* input, Color color) {
    if (!input) return;
    
    if (!input->custom_border_color) {
        input->custom_border_color = (Color*)malloc(sizeof(Color));
    }
    
    if (input->custom_border_color) {
        *input->custom_border_color = color;
    }
}

void TextInput_SetCursorColor(TextInput* input, Color color) {
    if (!input) return;
    
    if (!input->custom_cursor_color) {
        input->custom_cursor_color = (Color*)malloc(sizeof(Color));
    }
    
    if (input->custom_cursor_color) {
        *input->custom_cursor_color = color;
    }
}

void TextInput_SetSelectionColor(TextInput* input, Color color) {
    if (!input) return;
    
    if (!input->custom_selection_color) {
        input->custom_selection_color = (Color*)malloc(sizeof(Color));
    }
    
    if (input->custom_selection_color) {
        *input->custom_selection_color = color;
    }
}

void TextInput_SetPlaceholderColor(TextInput* input, Color color) {
    if (!input) return;
    
    if (!input->custom_placeholder_color) {
        input->custom_placeholder_color = (Color*)malloc(sizeof(Color));
    }
    
    if (input->custom_placeholder_color) {
        *input->custom_placeholder_color = color;
    }
}

void TextInput_SetPadding(TextInput* input, float top, float right, float bottom, float left) {
    if (!input) return;
    
    if (!input->custom_padding) {
        input->custom_padding = (EdgeValues*)malloc(sizeof(EdgeValues));
    }
    
    if (input->custom_padding) {
        input->custom_padding->top = top;
        input->custom_padding->right = right;
        input->custom_padding->bottom = bottom;
        input->custom_padding->left = left;
    }
}

void TextInput_SetFontSize(TextInput* input, int font_size) {
    if (!input) return;
    
    if (!input->custom_font_size) {
        input->custom_font_size = (int*)malloc(sizeof(int));
    }
    
    if (input->custom_font_size) {
        *input->custom_font_size = font_size;
    }
}

// Callback functions
void TextInput_SetOnTextChanged(TextInput* input, void (*callback)(const char*, void*), void* user_data) {
    if (!input) return;
    
    input->on_text_changed = callback;
    input->user_data = user_data;
}

void TextInput_SetOnEnterPressed(TextInput* input, void (*callback)(const char*, void*), void* user_data) {
    if (!input) return;
    
    input->on_enter_pressed = callback;
    input->user_data = user_data;
}

void TextInput_SetOnFocusChanged(TextInput* input, 
                                void (*on_focus_gained)(void*), 
                                void (*on_focus_lost)(void*), 
                                void* user_data) {
    if (!input) return;
    
    input->on_focus_gained = on_focus_gained;
    input->on_focus_lost = on_focus_lost;
    input->user_data = user_data;
}

// Utility functions
void TextInput_ResetCustomStyles(TextInput* input) {
    if (!input) return;
    
    if (input->custom_background_color) { free(input->custom_background_color); input->custom_background_color = NULL; }
    if (input->custom_text_color) { free(input->custom_text_color); input->custom_text_color = NULL; }
    if (input->custom_border_color) { free(input->custom_border_color); input->custom_border_color = NULL; }
    if (input->custom_cursor_color) { free(input->custom_cursor_color); input->custom_cursor_color = NULL; }
    if (input->custom_selection_color) { free(input->custom_selection_color); input->custom_selection_color = NULL; }
    if (input->custom_placeholder_color) { free(input->custom_placeholder_color); input->custom_placeholder_color = NULL; }
    if (input->custom_padding) { free(input->custom_padding); input->custom_padding = NULL; }
    if (input->custom_font_size) { free(input->custom_font_size); input->custom_font_size = NULL; }
}

// Built-in validators
bool TextInput_ValidateEmail(const char* text) {
    if (!text || strlen(text) == 0) return false;
    
    // Simple email validation
    bool has_at = false;
    bool has_dot_after_at = false;
    int at_pos = -1;
    
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '@') {
            if (has_at) return false; // Multiple @ symbols
            has_at = true;
            at_pos = i;
        } else if (text[i] == '.' && has_at && i > at_pos + 1) {
            has_dot_after_at = true;
        }
    }
    
    return has_at && has_dot_after_at && at_pos > 0 && strlen(text) > at_pos + 3;
}

bool TextInput_ValidateNumber(const char* text) {
    if (!text || strlen(text) == 0) return false;
    
    bool has_decimal = false;
    int start = 0;
    
    // Allow leading + or -
    if (text[0] == '+' || text[0] == '-') {
        start = 1;
    }
    
    if (text[start] == '\0') return false; // Only sign character
    
    for (int i = start; text[i] != '\0'; i++) {
        if (text[i] == '.') {
            if (has_decimal) return false; // Multiple decimal points
            has_decimal = true;
        } else if (!isdigit(text[i])) {
            return false;
        }
    }
    
    return true;
}

bool TextInput_ValidateNotEmpty(const char* text) {
    return text && strlen(text) > 0;
}