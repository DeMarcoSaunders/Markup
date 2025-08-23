#ifndef TEXT_AREA_H
#define TEXT_AREA_H

#include "raylib.h"
#include "theme.h"
#include "vendor/vec.h"
#include <stdbool.h>

typedef struct {
    char* text;
    int length;
} TextLine;

typedef struct {
    Rectangle bounds;
    char* text;
    char* placeholder;
    vec_t(TextLine) lines;  // Dynamic array of text lines
    int max_length;
    
    // State
    bool is_focused;
    bool is_hovered;
    bool is_disabled;
    bool is_readonly;
    bool is_visible;
    
    // Cursor and selection
    int cursor_line;
    int cursor_column;
    int selection_start_line;
    int selection_start_column;
    int selection_end_line;
    int selection_end_column;
    float cursor_blink_timer;
    bool cursor_visible;
    
    // Scrolling
    float scroll_x;
    float scroll_y;
    float max_scroll_x;
    float max_scroll_y;
    bool show_scroll_bars;
    
    // Scroll bar interaction
    bool dragging_v_scrollbar;
    bool dragging_h_scrollbar;
    float scrollbar_drag_offset;
    
    // Text rendering
    float line_height;
    float char_width;
    int visible_lines;
    int visible_chars;
    
    // Configuration
    bool word_wrap;
    bool show_line_numbers;
    bool auto_indent;
    int tab_size;
    
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
    Color* custom_line_number_color;
    Color* custom_scrollbar_color;
    EdgeValues* custom_padding;
    int* custom_font_size;
    float* custom_line_height;
    
    // Callbacks
    void (*on_text_changed)(const char* text, void* user_data);
    void (*on_line_changed)(int line, const char* line_text, void* user_data);
    void (*on_focus_gained)(void* user_data);
    void (*on_focus_lost)(void* user_data);
    void* user_data;
} TextArea;

// Creation and destruction
TextArea TextArea_Create(Rectangle bounds, const char* placeholder);
void TextArea_Destroy(TextArea* area);

// Update and drawing
void TextArea_Update(TextArea* area);
void TextArea_Draw(TextArea* area);

// Text management
void TextArea_SetText(TextArea* area, const char* text);
const char* TextArea_GetText(TextArea* area);
void TextArea_Clear(TextArea* area);
void TextArea_SetPlaceholder(TextArea* area, const char* placeholder);

// Line management
int TextArea_GetLineCount(TextArea* area);
const char* TextArea_GetLine(TextArea* area, int line_index);
void TextArea_SetLine(TextArea* area, int line_index, const char* text);
void TextArea_InsertLine(TextArea* area, int line_index, const char* text);
void TextArea_DeleteLine(TextArea* area, int line_index);

// State management
void TextArea_SetFocus(TextArea* area, bool focused);
bool TextArea_IsFocused(TextArea* area);
void TextArea_SetDisabled(TextArea* area, bool disabled);
void TextArea_SetReadonly(TextArea* area, bool readonly);
void TextArea_SetVisible(TextArea* area, bool visible);

// Selection and cursor
void TextArea_SelectAll(TextArea* area);
void TextArea_ClearSelection(TextArea* area);
void TextArea_SetCursorPosition(TextArea* area, int line, int column);
void TextArea_GetCursorPosition(TextArea* area, int* line, int* column);
bool TextArea_HasSelection(TextArea* area);
char* TextArea_GetSelectedText(TextArea* area);

// Scrolling
void TextArea_ScrollToLine(TextArea* area, int line);
void TextArea_ScrollToCursor(TextArea* area);
void TextArea_SetScrollPosition(TextArea* area, float scroll_x, float scroll_y);
void TextArea_GetScrollPosition(TextArea* area, float* scroll_x, float* scroll_y);

// Configuration
void TextArea_SetMaxLength(TextArea* area, int max_length);
void TextArea_SetWordWrap(TextArea* area, bool word_wrap);
void TextArea_SetShowLineNumbers(TextArea* area, bool show);
void TextArea_SetAutoIndent(TextArea* area, bool auto_indent);
void TextArea_SetTabSize(TextArea* area, int tab_size);
void TextArea_SetValidator(TextArea* area, bool (*validator)(const char*));
bool TextArea_IsValid(TextArea* area);
void TextArea_SetErrorMessage(TextArea* area, const char* message);

// Visual customization
void TextArea_SetBorder(TextArea* area, bool show_border);
void TextArea_SetShadow(TextArea* area, bool show_shadow);
void TextArea_SetShowPlaceholder(TextArea* area, bool show);
void TextArea_SetShowScrollBars(TextArea* area, bool show);

// Styling
void TextArea_SetBackgroundColor(TextArea* area, Color color);
void TextArea_SetTextColor(TextArea* area, Color color);
void TextArea_SetBorderColor(TextArea* area, Color color);
void TextArea_SetCursorColor(TextArea* area, Color color);
void TextArea_SetSelectionColor(TextArea* area, Color color);
void TextArea_SetPlaceholderColor(TextArea* area, Color color);
void TextArea_SetLineNumberColor(TextArea* area, Color color);
void TextArea_SetScrollbarColor(TextArea* area, Color color);
void TextArea_SetPadding(TextArea* area, float top, float right, float bottom, float left);
void TextArea_SetFontSize(TextArea* area, int font_size);
void TextArea_SetLineHeight(TextArea* area, float line_height);

// Callbacks
void TextArea_SetOnTextChanged(TextArea* area, void (*callback)(const char*, void*), void* user_data);
void TextArea_SetOnLineChanged(TextArea* area, void (*callback)(int, const char*, void*), void* user_data);
void TextArea_SetOnFocusChanged(TextArea* area, 
                               void (*on_focus_gained)(void*), 
                               void (*on_focus_lost)(void*), 
                               void* user_data);

// Utility functions
void TextArea_ResetCustomStyles(TextArea* area);
void TextArea_InsertText(TextArea* area, const char* text);
void TextArea_DeleteSelection(TextArea* area);

// File operations
bool TextArea_LoadFromFile(TextArea* area, const char* filename);
bool TextArea_SaveToFile(TextArea* area, const char* filename);

// Built-in validators
bool TextArea_ValidateNotEmpty(const char* text);
bool TextArea_ValidateMaxLines(const char* text, int max_lines);

#endif // TEXT_AREA_H