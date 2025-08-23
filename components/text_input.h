#ifndef TEXT_INPUT_H
#define TEXT_INPUT_H

#include "raylib.h"
#include "theme.h"
#include <stdbool.h>

typedef enum {
    INPUT_TYPE_TEXT,
    INPUT_TYPE_PASSWORD,
    INPUT_TYPE_NUMBER,
    INPUT_TYPE_EMAIL,
    INPUT_TYPE_MULTILINE
} TextInputType;

typedef struct {
    Rectangle bounds;
    char* text;
    char* placeholder;
    int max_length;
    TextInputType type;
    
    // State
    bool is_focused;
    bool is_hovered;
    bool is_disabled;
    bool is_readonly;
    bool is_visible;
    
    // Cursor and selection
    int cursor_position;
    int selection_start;
    int selection_end;
    float cursor_blink_timer;
    bool cursor_visible;
    
    // Scrolling (for long text)
    float scroll_offset;
    float text_width;
    
    // Validation
    bool (*validator)(const char* text);
    bool is_valid;
    char* error_message;
    
    // Visual customization
    bool show_border;
    bool show_shadow;
    bool show_placeholder;
    
    // Custom styling (optional overrides)
    Color* custom_background_color;
    Color* custom_text_color;
    Color* custom_border_color;
    Color* custom_cursor_color;
    Color* custom_selection_color;
    Color* custom_placeholder_color;
    EdgeValues* custom_padding;
    int* custom_font_size;
    
    // Callbacks
    void (*on_text_changed)(const char* text, void* user_data);
    void (*on_enter_pressed)(const char* text, void* user_data);
    void (*on_focus_gained)(void* user_data);
    void (*on_focus_lost)(void* user_data);
    void* user_data;
} TextInput;

// Creation and destruction
TextInput TextInput_Create(Rectangle bounds, const char* placeholder);
TextInput TextInput_CreateWithType(Rectangle bounds, const char* placeholder, TextInputType type);
void TextInput_Destroy(TextInput* input);

// Update and drawing
void TextInput_Update(TextInput* input);
void TextInput_Draw(TextInput* input);

// Text management
void TextInput_SetText(TextInput* input, const char* text);
const char* TextInput_GetText(TextInput* input);
void TextInput_Clear(TextInput* input);
void TextInput_SetPlaceholder(TextInput* input, const char* placeholder);

// State management
void TextInput_SetFocus(TextInput* input, bool focused);
bool TextInput_IsFocused(TextInput* input);
void TextInput_SetDisabled(TextInput* input, bool disabled);
void TextInput_SetReadonly(TextInput* input, bool readonly);
void TextInput_SetVisible(TextInput* input, bool visible);

// Selection and cursor
void TextInput_SelectAll(TextInput* input);
void TextInput_ClearSelection(TextInput* input);
void TextInput_SetCursorPosition(TextInput* input, int position);
int TextInput_GetCursorPosition(TextInput* input);
bool TextInput_HasSelection(TextInput* input);
char* TextInput_GetSelectedText(TextInput* input);

// Configuration
void TextInput_SetMaxLength(TextInput* input, int max_length);
void TextInput_SetType(TextInput* input, TextInputType type);
void TextInput_SetValidator(TextInput* input, bool (*validator)(const char*));
bool TextInput_IsValid(TextInput* input);
void TextInput_SetErrorMessage(TextInput* input, const char* message);

// Visual customization
void TextInput_SetBorder(TextInput* input, bool show_border);
void TextInput_SetShadow(TextInput* input, bool show_shadow);
void TextInput_SetShowPlaceholder(TextInput* input, bool show);

// Styling
void TextInput_SetBackgroundColor(TextInput* input, Color color);
void TextInput_SetTextColor(TextInput* input, Color color);
void TextInput_SetBorderColor(TextInput* input, Color color);
void TextInput_SetCursorColor(TextInput* input, Color color);
void TextInput_SetSelectionColor(TextInput* input, Color color);
void TextInput_SetPlaceholderColor(TextInput* input, Color color);
void TextInput_SetPadding(TextInput* input, float top, float right, float bottom, float left);
void TextInput_SetFontSize(TextInput* input, int font_size);

// Callbacks
void TextInput_SetOnTextChanged(TextInput* input, void (*callback)(const char*, void*), void* user_data);
void TextInput_SetOnEnterPressed(TextInput* input, void (*callback)(const char*, void*), void* user_data);
void TextInput_SetOnFocusChanged(TextInput* input, 
                                void (*on_focus_gained)(void*), 
                                void (*on_focus_lost)(void*), 
                                void* user_data);

// Utility functions
void TextInput_ResetCustomStyles(TextInput* input);

// Built-in validators
bool TextInput_ValidateEmail(const char* text);
bool TextInput_ValidateNumber(const char* text);
bool TextInput_ValidateNotEmpty(const char* text);

#endif // TEXT_INPUT_H