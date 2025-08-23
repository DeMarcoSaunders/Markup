#include "text_area.h"
#include "shadow.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

// Helper function to get effective style values
static ComponentStyle* GetEffectiveStyle(TextArea* area) {
    return Theme_GetComponentStyle(COMPONENT_TEXT_AREA);
}

static ComponentState GetCurrentState(TextArea* area) {
    if (area->is_disabled) return STATE_DISABLED;
    if (area->is_focused) return STATE_ACTIVE;
    if (area->is_hovered) return STATE_HOVER;
    return STATE_DEFAULT;
}

// Helper function to split text into lines
static void SplitTextIntoLines(TextArea* area, const char* text) {
    // Clear existing lines
    for (int i = 0; i < area->lines.length; i++) {
        if (area->lines.data[i].text) {
            free(area->lines.data[i].text);
        }
    }
    vec_clear(&area->lines);
    
    if (!text || strlen(text) == 0) {
        // Add empty line
        TextLine empty_line = {0};
        empty_line.text = (char*)calloc(1, sizeof(char));
        empty_line.length = 0;
        vec_push(&area->lines, empty_line);
        return;
    }
    
    const char* line_start = text;
    const char* current = text;
    
    while (*current != '\0') {
        if (*current == '\n' || *current == '\r') {
            // Found line ending
            int line_length = current - line_start;
            
            TextLine line = {0};
            line.text = (char*)malloc(line_length + 1);
            strncpy(line.text, line_start, line_length);
            line.text[line_length] = '\0';
            line.length = line_length;
            vec_push(&area->lines, line);
            
            // Skip \r\n or \n\r combinations
            if (*current == '\r' && *(current + 1) == '\n') {
                current += 2;
            } else if (*current == '\n' && *(current + 1) == '\r') {
                current += 2;
            } else {
                current++;
            }
            
            line_start = current;
        } else {
            current++;
        }
    }
    
    // Add final line if there's remaining text
    if (line_start < current) {
        int line_length = current - line_start;
        TextLine line = {0};
        line.text = (char*)malloc(line_length + 1);
        strncpy(line.text, line_start, line_length);
        line.text[line_length] = '\0';
        line.length = line_length;
        vec_push(&area->lines, line);
    }
    
    // Ensure at least one line exists
    if (area->lines.length == 0) {
        TextLine empty_line = {0};
        empty_line.text = (char*)calloc(1, sizeof(char));
        empty_line.length = 0;
        vec_push(&area->lines, empty_line);
    }
}

// Helper function to rebuild full text from lines
static void RebuildTextFromLines(TextArea* area) {
    if (area->text) {
        free(area->text);
        area->text = NULL;
    }
    
    // Calculate total length needed
    int total_length = 0;
    for (int i = 0; i < area->lines.length; i++) {
        total_length += area->lines.data[i].length;
        if (i < area->lines.length - 1) {
            total_length++; // For newline character
        }
    }
    
    area->text = (char*)malloc(total_length + 1);
    area->text[0] = '\0';
    
    for (int i = 0; i < area->lines.length; i++) {
        strcat(area->text, area->lines.data[i].text);
        if (i < area->lines.length - 1) {
            strcat(area->text, "\n");
        }
    }
}

// Helper function to calculate text metrics
static void UpdateTextMetrics(TextArea* area) {
    ComponentStyle* style = GetEffectiveStyle(area);
    int font_size = area->custom_font_size ? *area->custom_font_size : style->font_size;
    float line_height = area->custom_line_height ? *area->custom_line_height : 
                       (font_size * AppTheme.text_area_props.line_height);
    
    area->line_height = line_height;
    area->char_width = MeasureTextEx(GetFontDefault(), "W", font_size, 1.0f).x; // Use 'W' for average char width
    
    EdgeValues padding = area->custom_padding ? *area->custom_padding : style->padding;
    float content_height = area->bounds.height - padding.top - padding.bottom;
    float content_width = area->bounds.width - padding.left - padding.right;
    
    if (area->show_line_numbers) {
        content_width -= 50; // Reserve space for line numbers
    }
    
    area->visible_lines = (int)(content_height / line_height);
    area->visible_chars = (int)(content_width / area->char_width);
    
    // Calculate max scroll values
    area->max_scroll_y = fmaxf(0, (area->lines.length * line_height) - content_height);
    
    float max_line_width = 0;
    for (int i = 0; i < area->lines.length; i++) {
        Vector2 line_size = MeasureTextEx(GetFontDefault(), area->lines.data[i].text, font_size, 1.0f);
        if (line_size.x > max_line_width) {
            max_line_width = line_size.x;
        }
    }
    area->max_scroll_x = fmaxf(0, max_line_width - content_width);
}

// Helper function to get cursor screen position
static Vector2 GetCursorScreenPosition(TextArea* area) {
    ComponentStyle* style = GetEffectiveStyle(area);
    EdgeValues padding = area->custom_padding ? *area->custom_padding : style->padding;
    int font_size = area->custom_font_size ? *area->custom_font_size : style->font_size;
    
    float line_number_width = area->show_line_numbers ? 50 : 0;
    float x = area->bounds.x + padding.left + line_number_width - area->scroll_x;
    float y = area->bounds.y + padding.top + (area->cursor_line * area->line_height) - area->scroll_y;
    
    // Calculate x position based on cursor column
    if (area->cursor_line < area->lines.length && area->cursor_column > 0) {
        char temp_char = area->lines.data[area->cursor_line].text[area->cursor_column];
        area->lines.data[area->cursor_line].text[area->cursor_column] = '\0';
        
        Vector2 text_size = MeasureTextEx(GetFontDefault(), area->lines.data[area->cursor_line].text, font_size, 1.0f);
        x += text_size.x;
        
        area->lines.data[area->cursor_line].text[area->cursor_column] = temp_char;
    }
    
    return (Vector2){x, y};
}

// Helper function to get line/column from screen position
static void GetLineColumnFromPosition(TextArea* area, Vector2 pos, int* line, int* column) {
    ComponentStyle* style = GetEffectiveStyle(area);
    EdgeValues padding = area->custom_padding ? *area->custom_padding : style->padding;
    int font_size = area->custom_font_size ? *area->custom_font_size : style->font_size;
    
    float line_number_width = area->show_line_numbers ? 50 : 0;
    float relative_y = pos.y - area->bounds.y - padding.top + area->scroll_y;
    float relative_x = pos.x - area->bounds.x - padding.left - line_number_width + area->scroll_x;
    
    *line = (int)(relative_y / area->line_height);
    *line = *line < 0 ? 0 : (*line >= area->lines.length ? area->lines.length - 1 : *line);
    
    if (*line < area->lines.length) {
        const char* line_text = area->lines.data[*line].text;
        int line_length = area->lines.data[*line].length;
        
        *column = 0;
        for (int i = 0; i <= line_length; i++) {
            char temp_char = line_text[i];
            ((char*)line_text)[i] = '\0';
            
            Vector2 text_size = MeasureTextEx(GetFontDefault(), line_text, font_size, 1.0f);
            ((char*)line_text)[i] = temp_char;
            
            if (text_size.x >= relative_x) {
                *column = i;
                break;
            }
            
            if (i == line_length) {
                *column = line_length;
            }
        }
    } else {
        *column = 0;
    }
}

// Helper function to ensure cursor is visible
static void ScrollToCursor(TextArea* area) {
    Vector2 cursor_pos = GetCursorScreenPosition(area);
    ComponentStyle* style = GetEffectiveStyle(area);
    EdgeValues padding = area->custom_padding ? *area->custom_padding : style->padding;
    
    Rectangle content_area = {
        area->bounds.x + padding.left + (area->show_line_numbers ? 50 : 0),
        area->bounds.y + padding.top,
        area->bounds.width - padding.left - padding.right - (area->show_line_numbers ? 50 : 0),
        area->bounds.height - padding.top - padding.bottom
    };
    
    // Vertical scrolling
    if (cursor_pos.y < content_area.y) {
        area->scroll_y -= content_area.y - cursor_pos.y + area->line_height;
    } else if (cursor_pos.y + area->line_height > content_area.y + content_area.height) {
        area->scroll_y += (cursor_pos.y + area->line_height) - (content_area.y + content_area.height) + area->line_height;
    }
    
    // Horizontal scrolling
    if (cursor_pos.x < content_area.x) {
        area->scroll_x -= content_area.x - cursor_pos.x + area->char_width * 2;
    } else if (cursor_pos.x > content_area.x + content_area.width) {
        area->scroll_x += cursor_pos.x - (content_area.x + content_area.width) + area->char_width * 2;
    }
    
    // Clamp scroll values
    area->scroll_x = fmaxf(0, fminf(area->scroll_x, area->max_scroll_x));
    area->scroll_y = fmaxf(0, fminf(area->scroll_y, area->max_scroll_y));
}

// Helper function to insert character at cursor
static void InsertCharacterAtCursor(TextArea* area, char c) {
    if (area->is_readonly || area->is_disabled) return;
    if (area->cursor_line >= area->lines.length) return;
    
    TextLine* line = &area->lines.data[area->cursor_line];
    
    // Check if we need to expand the line buffer
    char* new_text = (char*)realloc(line->text, line->length + 2);
    if (!new_text) return;
    
    line->text = new_text;
    
    // Insert character
    memmove(line->text + area->cursor_column + 1, 
            line->text + area->cursor_column, 
            line->length - area->cursor_column + 1);
    line->text[area->cursor_column] = c;
    line->length++;
    
    area->cursor_column++;
    
    // Rebuild full text and trigger callbacks
    RebuildTextFromLines(area);
    UpdateTextMetrics(area);
    ScrollToCursor(area);
    
    if (area->validator) {
        area->is_valid = area->validator(area->text);
    }
    
    if (area->on_text_changed) {
        area->on_text_changed(area->text, area->user_data);
    }
    
    if (area->on_line_changed) {
        area->on_line_changed(area->cursor_line, line->text, area->user_data);
    }
}

// Helper function to insert new line
static void InsertNewLine(TextArea* area) {
    if (area->is_readonly || area->is_disabled) return;
    if (area->cursor_line >= area->lines.length) return;
    
    TextLine* current_line = &area->lines.data[area->cursor_line];
    
    // Split current line at cursor position
    int remaining_length = current_line->length - area->cursor_column;
    char* remaining_text = (char*)malloc(remaining_length + 1);
    strcpy(remaining_text, current_line->text + area->cursor_column);
    
    // Truncate current line
    current_line->text[area->cursor_column] = '\0';
    current_line->length = area->cursor_column;
    current_line->text = (char*)realloc(current_line->text, current_line->length + 1);
    
    // Create new line
    TextLine new_line = {0};
    new_line.text = remaining_text;
    new_line.length = remaining_length;
    
    // Auto-indent if enabled
    if (area->auto_indent && area->cursor_line < area->lines.length) {
        int indent_count = 0;
        const char* original_line = area->lines.data[area->cursor_line].text;
        while (indent_count < area->lines.data[area->cursor_line].length && 
               (original_line[indent_count] == ' ' || original_line[indent_count] == '\t')) {
            indent_count++;
        }
        
        if (indent_count > 0) {
            char* indented_text = (char*)malloc(new_line.length + indent_count + 1);
            strncpy(indented_text, original_line, indent_count);
            strcpy(indented_text + indent_count, new_line.text);
            
            free(new_line.text);
            new_line.text = indented_text;
            new_line.length += indent_count;
        }
    }
    
    // Insert new line into vector
    vec_insert(&area->lines, area->cursor_line + 1, new_line);
    
    // Move cursor to beginning of new line (after indentation)
    area->cursor_line++;
    area->cursor_column = new_line.length - remaining_length; // Position after auto-indent
    
    // Rebuild and update
    RebuildTextFromLines(area);
    UpdateTextMetrics(area);
    ScrollToCursor(area);
    
    if (area->validator) {
        area->is_valid = area->validator(area->text);
    }
    
    if (area->on_text_changed) {
        area->on_text_changed(area->text, area->user_data);
    }
}

TextArea TextArea_Create(Rectangle bounds, const char* placeholder) {
    TextArea area = {0};
    
    area.bounds = bounds;
    area.max_length = 10000; // Default max length for text areas
    
    // Initialize lines vector
    vec_init(&area.lines);
    
    // Set placeholder
    if (placeholder) {
        size_t placeholder_len = strlen(placeholder) + 1;
        area.placeholder = (char*)malloc(placeholder_len);
        strcpy(area.placeholder, placeholder);
    }
    
    // Initialize with empty text
    area.text = (char*)calloc(1, sizeof(char));
    SplitTextIntoLines(&area, "");
    
    // Initialize state
    area.is_focused = false;
    area.is_hovered = false;
    area.is_disabled = false;
    area.is_readonly = false;
    area.is_visible = true;
    
    // Initialize cursor and selection
    area.cursor_line = 0;
    area.cursor_column = 0;
    area.selection_start_line = 0;
    area.selection_start_column = 0;
    area.selection_end_line = 0;
    area.selection_end_column = 0;
    area.cursor_blink_timer = 0.0f;
    area.cursor_visible = true;
    
    // Initialize scrolling
    area.scroll_x = 0.0f;
    area.scroll_y = 0.0f;
    area.max_scroll_x = 0.0f;
    area.max_scroll_y = 0.0f;
    area.show_scroll_bars = AppTheme.text_area_props.show_scroll_bars;
    area.dragging_v_scrollbar = false;
    area.dragging_h_scrollbar = false;
    
    // Initialize configuration
    area.word_wrap = AppTheme.text_area_props.word_wrap;
    area.show_line_numbers = AppTheme.text_area_props.show_line_numbers;
    area.auto_indent = AppTheme.text_area_props.auto_indent;
    area.tab_size = AppTheme.text_area_props.tab_size;
    
    // Initialize validation
    area.validator = NULL;
    area.is_valid = true;
    area.error_message = NULL;
    
    // Initialize visual settings
    area.show_border = true;
    area.show_shadow = false;
    area.show_placeholder = true;
    
    // Initialize custom styling pointers to NULL
    area.custom_background_color = NULL;
    area.custom_text_color = NULL;
    area.custom_border_color = NULL;
    area.custom_cursor_color = NULL;
    area.custom_selection_color = NULL;
    area.custom_placeholder_color = NULL;
    area.custom_line_number_color = NULL;
    area.custom_scrollbar_color = NULL;
    area.custom_padding = NULL;
    area.custom_font_size = NULL;
    area.custom_line_height = NULL;
    
    // Initialize callbacks
    area.on_text_changed = NULL;
    area.on_line_changed = NULL;
    area.on_focus_gained = NULL;
    area.on_focus_lost = NULL;
    area.user_data = NULL;
    
    // Calculate initial metrics
    UpdateTextMetrics(&area);
    
    return area;
}

void TextArea_Destroy(TextArea* area) {
    if (!area) return;
    
    // Free text
    if (area->text) {
        free(area->text);
        area->text = NULL;
    }
    
    // Free placeholder
    if (area->placeholder) {
        free(area->placeholder);
        area->placeholder = NULL;
    }
    
    // Free error message
    if (area->error_message) {
        free(area->error_message);
        area->error_message = NULL;
    }
    
    // Free lines
    for (int i = 0; i < area->lines.length; i++) {
        if (area->lines.data[i].text) {
            free(area->lines.data[i].text);
        }
    }
    vec_deinit(&area->lines);
    
    // Free custom styling
    if (area->custom_background_color) { free(area->custom_background_color); area->custom_background_color = NULL; }
    if (area->custom_text_color) { free(area->custom_text_color); area->custom_text_color = NULL; }
    if (area->custom_border_color) { free(area->custom_border_color); area->custom_border_color = NULL; }
    if (area->custom_cursor_color) { free(area->custom_cursor_color); area->custom_cursor_color = NULL; }
    if (area->custom_selection_color) { free(area->custom_selection_color); area->custom_selection_color = NULL; }
    if (area->custom_placeholder_color) { free(area->custom_placeholder_color); area->custom_placeholder_color = NULL; }
    if (area->custom_line_number_color) { free(area->custom_line_number_color); area->custom_line_number_color = NULL; }
    if (area->custom_scrollbar_color) { free(area->custom_scrollbar_color); area->custom_scrollbar_color = NULL; }
    if (area->custom_padding) { free(area->custom_padding); area->custom_padding = NULL; }
    if (area->custom_font_size) { free(area->custom_font_size); area->custom_font_size = NULL; }
    if (area->custom_line_height) { free(area->custom_line_height); area->custom_line_height = NULL; }
}

void TextArea_Update(TextArea* area) {
    if (!area || !area->is_visible) return;
    
    Vector2 mouse_pos = GetMousePosition();
    bool mouse_over = CheckCollisionPointRec(mouse_pos, area->bounds);
    
    area->is_hovered = mouse_over && !area->is_disabled;
    
    // Handle focus
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        bool was_focused = area->is_focused;
        area->is_focused = mouse_over && !area->is_disabled;
        
        if (area->is_focused && !was_focused) {
            // Gained focus
            if (area->on_focus_gained) {
                area->on_focus_gained(area->user_data);
            }
        } else if (!area->is_focused && was_focused) {
            // Lost focus
            TextArea_ClearSelection(area);
            if (area->on_focus_lost) {
                area->on_focus_lost(area->user_data);
            }
        }
        
        // Set cursor position on click
        if (area->is_focused && mouse_over) {
            int new_line, new_column;
            GetLineColumnFromPosition(area, mouse_pos, &new_line, &new_column);
            
            area->cursor_line = new_line;
            area->cursor_column = new_column;
            
            if (!IsKeyDown(KEY_LEFT_SHIFT)) {
                TextArea_ClearSelection(area);
            }
        }
    }
    
    // Handle scrolling with mouse wheel
    if (mouse_over && !area->is_disabled) {
        float wheel_move = GetMouseWheelMove();
        if (wheel_move != 0) {
            if (IsKeyDown(KEY_LEFT_SHIFT)) {
                // Horizontal scroll
                area->scroll_x -= wheel_move * AppTheme.text_area_props.scroll_speed;
                area->scroll_x = fmaxf(0, fminf(area->scroll_x, area->max_scroll_x));
            } else {
                // Vertical scroll
                area->scroll_y -= wheel_move * AppTheme.text_area_props.scroll_speed;
                area->scroll_y = fmaxf(0, fminf(area->scroll_y, area->max_scroll_y));
            }
        }
    }
    
    if (!area->is_focused || area->is_disabled || area->is_readonly) {
        return;
    }
    
    // Handle text input
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126) { // Printable ASCII characters
            if (key == '\t') {
                // Handle tab as spaces
                for (int i = 0; i < area->tab_size; i++) {
                    InsertCharacterAtCursor(area, ' ');
                }
            } else {
                InsertCharacterAtCursor(area, (char)key);
            }
        }
        key = GetCharPressed();
    }
    
    // Handle special keys
    if (IsKeyPressed(KEY_ENTER)) {
        InsertNewLine(area);
    }
    
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (area->cursor_column > 0) {
            // Delete character before cursor
            TextLine* line = &area->lines.data[area->cursor_line];
            memmove(line->text + area->cursor_column - 1, 
                    line->text + area->cursor_column, 
                    line->length - area->cursor_column + 1);
            line->length--;
            line->text = (char*)realloc(line->text, line->length + 1);
            area->cursor_column--;
            
            RebuildTextFromLines(area);
            UpdateTextMetrics(area);
        } else if (area->cursor_line > 0) {
            // Join with previous line
            TextLine* prev_line = &area->lines.data[area->cursor_line - 1];
            TextLine* curr_line = &area->lines.data[area->cursor_line];
            
            int new_cursor_column = prev_line->length;
            
            // Expand previous line to accommodate current line
            prev_line->text = (char*)realloc(prev_line->text, prev_line->length + curr_line->length + 1);
            strcat(prev_line->text, curr_line->text);
            prev_line->length += curr_line->length;
            
            // Remove current line
            free(curr_line->text);
            vec_splice(&area->lines, area->cursor_line, 1);
            
            area->cursor_line--;
            area->cursor_column = new_cursor_column;
            
            RebuildTextFromLines(area);
            UpdateTextMetrics(area);
        }
        ScrollToCursor(area);
    }
    
    if (IsKeyPressed(KEY_DELETE)) {
        if (area->cursor_line < area->lines.length) {
            TextLine* line = &area->lines.data[area->cursor_line];
            if (area->cursor_column < line->length) {
                // Delete character after cursor
                memmove(line->text + area->cursor_column, 
                        line->text + area->cursor_column + 1, 
                        line->length - area->cursor_column);
                line->length--;
                line->text = (char*)realloc(line->text, line->length + 1);
                
                RebuildTextFromLines(area);
                UpdateTextMetrics(area);
            } else if (area->cursor_line < area->lines.length - 1) {
                // Join with next line
                TextLine* curr_line = &area->lines.data[area->cursor_line];
                TextLine* next_line = &area->lines.data[area->cursor_line + 1];
                
                // Expand current line
                curr_line->text = (char*)realloc(curr_line->text, curr_line->length + next_line->length + 1);
                strcat(curr_line->text, next_line->text);
                curr_line->length += next_line->length;
                
                // Remove next line
                free(next_line->text);
                vec_splice(&area->lines, area->cursor_line + 1, 1);
                
                RebuildTextFromLines(area);
                UpdateTextMetrics(area);
            }
        }
        ScrollToCursor(area);
    }
    
    // Handle cursor movement
    if (IsKeyPressed(KEY_LEFT)) {
        if (area->cursor_column > 0) {
            area->cursor_column--;
        } else if (area->cursor_line > 0) {
            area->cursor_line--;
            area->cursor_column = area->lines.data[area->cursor_line].length;
        }
        ScrollToCursor(area);
    }
    
    if (IsKeyPressed(KEY_RIGHT)) {
        if (area->cursor_line < area->lines.length) {
            if (area->cursor_column < area->lines.data[area->cursor_line].length) {
                area->cursor_column++;
            } else if (area->cursor_line < area->lines.length - 1) {
                area->cursor_line++;
                area->cursor_column = 0;
            }
        }
        ScrollToCursor(area);
    }
    
    if (IsKeyPressed(KEY_UP)) {
        if (area->cursor_line > 0) {
            area->cursor_line--;
            int line_length = area->lines.data[area->cursor_line].length;
            if (area->cursor_column > line_length) {
                area->cursor_column = line_length;
            }
        }
        ScrollToCursor(area);
    }
    
    if (IsKeyPressed(KEY_DOWN)) {
        if (area->cursor_line < area->lines.length - 1) {
            area->cursor_line++;
            int line_length = area->lines.data[area->cursor_line].length;
            if (area->cursor_column > line_length) {
                area->cursor_column = line_length;
            }
        }
        ScrollToCursor(area);
    }
    
    if (IsKeyPressed(KEY_HOME)) {
        area->cursor_column = 0;
        ScrollToCursor(area);
    }
    
    if (IsKeyPressed(KEY_END)) {
        if (area->cursor_line < area->lines.length) {
            area->cursor_column = area->lines.data[area->cursor_line].length;
        }
        ScrollToCursor(area);
    }
    
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_HOME)) {
        area->cursor_line = 0;
        area->cursor_column = 0;
        ScrollToCursor(area);
    }
    
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_END)) {
        area->cursor_line = area->lines.length - 1;
        if (area->cursor_line >= 0) {
            area->cursor_column = area->lines.data[area->cursor_line].length;
        }
        ScrollToCursor(area);
    }
    
    // Handle Ctrl+A (Select All)
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) {
        TextArea_SelectAll(area);
    }
    
    // Update cursor blink
    area->cursor_blink_timer += GetFrameTime();
    if (area->cursor_blink_timer >= 1.0f / AppTheme.text_input_props.cursor_blink_speed) {
        area->cursor_visible = !area->cursor_visible;
        area->cursor_blink_timer = 0.0f;
    }
}

void TextArea_Draw(TextArea* area) {
    if (!area || !area->is_visible) return;
    
    ComponentStyle* style = GetEffectiveStyle(area);
    ComponentState state = GetCurrentState(area);
    ColorPalette* colors = &style->colors[state];
    
    // Get effective values
    EdgeValues padding = area->custom_padding ? *area->custom_padding : style->padding;
    int font_size = area->custom_font_size ? *area->custom_font_size : style->font_size;
    float line_height = area->custom_line_height ? *area->custom_line_height : area->line_height;
    
    Color bg_color = area->custom_background_color ? *area->custom_background_color : colors->background;
    Color text_color = area->custom_text_color ? *area->custom_text_color : colors->text;
    Color border_color = area->custom_border_color ? *area->custom_border_color : colors->border;
    Color cursor_color = area->custom_cursor_color ? *area->custom_cursor_color : colors->accent;
    Color selection_color = area->custom_selection_color ? *area->custom_selection_color : colors->accent;
    selection_color.a = (unsigned char)(selection_color.a * AppTheme.text_input_props.selection_opacity);
    
    // Draw shadow
    if (area->show_shadow) {
        ShadowConfig shadow_config = Shadow_FromTheme(style, state);
        Shadow_DrawRectangleRounded(area->bounds, style->border_radius.top_left, &shadow_config);
    }
    
    // Draw background
    DrawRectangleRounded(area->bounds, style->border_radius.top_left / area->bounds.height, 8, bg_color);
    
    // Draw border
    if (area->show_border) {
        DrawRectangleRoundedLines(area->bounds, style->border_radius.top_left / area->bounds.height, 
                                 8, border_color);
    }
    
    // Calculate content area
    float line_number_width = area->show_line_numbers ? 50 : 0;
    Rectangle content_area = {
        area->bounds.x + padding.left + line_number_width,
        area->bounds.y + padding.top,
        area->bounds.width - padding.left - padding.right - line_number_width,
        area->bounds.height - padding.top - padding.bottom
    };
    
    // Begin scissor mode for content clipping
    BeginScissorMode((int)content_area.x, (int)content_area.y, (int)content_area.width, (int)content_area.height);
    
    // Draw text lines
    if (area->lines.length > 0 && (area->text && strlen(area->text) > 0)) {
        int start_line = (int)(area->scroll_y / line_height);
        int end_line = start_line + area->visible_lines + 1;
        
        start_line = start_line < 0 ? 0 : start_line;
        end_line = end_line >= area->lines.length ? area->lines.length : end_line;
        
        for (int i = start_line; i < end_line; i++) {
            Vector2 line_pos = {
                content_area.x - area->scroll_x,
                content_area.y + (i * line_height) - area->scroll_y
            };
            
            DrawTextEx(GetFontDefault(), area->lines.data[i].text, line_pos, font_size, 1.0f, text_color);
        }
    } else if (area->show_placeholder && area->placeholder && !area->is_focused) {
        // Draw placeholder text
        Color placeholder_color = area->custom_placeholder_color ? 
                                 *area->custom_placeholder_color : 
                                 (Color){text_color.r, text_color.g, text_color.b, (unsigned char)(text_color.a * 0.5f)};
        
        Vector2 placeholder_pos = {content_area.x, content_area.y};
        DrawTextEx(GetFontDefault(), area->placeholder, placeholder_pos, font_size, 1.0f, placeholder_color);
    }
    
    // Draw cursor
    if (area->is_focused && area->cursor_visible && !area->is_disabled) {
        Vector2 cursor_screen_pos = GetCursorScreenPosition(area);
        float cursor_width = AppTheme.text_input_props.cursor_width;
        
        DrawRectangle((int)cursor_screen_pos.x, (int)cursor_screen_pos.y, 
                     (int)cursor_width, (int)line_height, cursor_color);
    }
    
    EndScissorMode();
    
    // Draw line numbers
    if (area->show_line_numbers) {
        Rectangle line_number_area = {
            area->bounds.x + padding.left,
            area->bounds.y + padding.top,
            line_number_width - 5,
            area->bounds.height - padding.top - padding.bottom
        };
        
        BeginScissorMode((int)line_number_area.x, (int)line_number_area.y, 
                        (int)line_number_area.width, (int)line_number_area.height);
        
        Color line_number_color = area->custom_line_number_color ? 
                                 *area->custom_line_number_color : 
                                 (Color){text_color.r, text_color.g, text_color.b, (unsigned char)(text_color.a * 0.6f)};
        
        int start_line = (int)(area->scroll_y / line_height);
        int end_line = start_line + area->visible_lines + 1;
        
        start_line = start_line < 0 ? 0 : start_line;
        end_line = end_line >= area->lines.length ? area->lines.length : end_line;
        
        for (int i = start_line; i < end_line; i++) {
            char line_num_str[16];
            sprintf(line_num_str, "%d", i + 1);
            
            Vector2 line_num_pos = {
                line_number_area.x + line_number_area.width - MeasureTextEx(GetFontDefault(), line_num_str, font_size - 2, 1.0f).x,
                line_number_area.y + (i * line_height) - area->scroll_y
            };
            
            DrawTextEx(GetFontDefault(), line_num_str, line_num_pos, font_size - 2, 1.0f, line_number_color);
        }
        
        EndScissorMode();
        
        // Draw separator line
        DrawLine((int)(area->bounds.x + padding.left + line_number_width - 2), 
                (int)(area->bounds.y + padding.top),
                (int)(area->bounds.x + padding.left + line_number_width - 2), 
                (int)(area->bounds.y + area->bounds.height - padding.bottom),
                line_number_color);
    }
    
    // Draw scroll bars
    if (area->show_scroll_bars) {
        Color scrollbar_color = area->custom_scrollbar_color ? 
                               *area->custom_scrollbar_color : 
                               (Color){colors->border.r, colors->border.g, colors->border.b, 150};
        
        // Vertical scrollbar
        if (area->max_scroll_y > 0) {
            float scrollbar_height = (content_area.height / (area->max_scroll_y + content_area.height)) * content_area.height;
            float scrollbar_y = area->bounds.y + padding.top + (area->scroll_y / area->max_scroll_y) * (content_area.height - scrollbar_height);
            
            Rectangle v_scrollbar = {
                area->bounds.x + area->bounds.width - AppTheme.text_area_props.scroll_bar_width - 2,
                scrollbar_y,
                AppTheme.text_area_props.scroll_bar_width,
                scrollbar_height
            };
            
            DrawRectangleRounded(v_scrollbar, 0.5f, 4, scrollbar_color);
        }
        
        // Horizontal scrollbar
        if (area->max_scroll_x > 0) {
            float scrollbar_width = (content_area.width / (area->max_scroll_x + content_area.width)) * content_area.width;
            float scrollbar_x = area->bounds.x + padding.left + line_number_width + (area->scroll_x / area->max_scroll_x) * (content_area.width - scrollbar_width);
            
            Rectangle h_scrollbar = {
                scrollbar_x,
                area->bounds.y + area->bounds.height - AppTheme.text_area_props.scroll_bar_width - 2,
                scrollbar_width,
                AppTheme.text_area_props.scroll_bar_width
            };
            
            DrawRectangleRounded(h_scrollbar, 0.5f, 4, scrollbar_color);
        }
    }
    
    // Draw error message if invalid
    if (!area->is_valid && area->error_message) {
        Vector2 error_pos = {area->bounds.x, area->bounds.y + area->bounds.height + 5};
        DrawTextEx(GetFontDefault(), area->error_message, error_pos, font_size - 2, 1.0f, RED);
    }
}

// Text management functions
void TextArea_SetText(TextArea* area, const char* text) {
    if (!area) return;
    
    SplitTextIntoLines(area, text);
    RebuildTextFromLines(area);
    
    // Reset cursor position
    area->cursor_line = 0;
    area->cursor_column = 0;
    TextArea_ClearSelection(area);
    
    // Update metrics and scroll
    UpdateTextMetrics(area);
    area->scroll_x = area->scroll_y = 0;
    
    // Validate
    if (area->validator) {
        area->is_valid = area->validator(area->text);
    }
    
    // Trigger callback
    if (area->on_text_changed) {
        area->on_text_changed(area->text, area->user_data);
    }
}

const char* TextArea_GetText(TextArea* area) {
    return area ? area->text : NULL;
}

void TextArea_Clear(TextArea* area) {
    TextArea_SetText(area, "");
}

void TextArea_SetPlaceholder(TextArea* area, const char* placeholder) {
    if (!area) return;
    
    if (area->placeholder) {
        free(area->placeholder);
        area->placeholder = NULL;
    }
    
    if (placeholder) {
        size_t placeholder_len = strlen(placeholder) + 1;
        area->placeholder = (char*)malloc(placeholder_len);
        strcpy(area->placeholder, placeholder);
    }
}

// Line management functions
int TextArea_GetLineCount(TextArea* area) {
    return area ? area->lines.length : 0;
}

const char* TextArea_GetLine(TextArea* area, int line_index) {
    if (!area || line_index < 0 || line_index >= area->lines.length) {
        return NULL;
    }
    return area->lines.data[line_index].text;
}

void TextArea_SetLine(TextArea* area, int line_index, const char* text) {
    if (!area || line_index < 0 || line_index >= area->lines.length || !text) return;
    
    TextLine* line = &area->lines.data[line_index];
    
    // Free old text and set new
    free(line->text);
    line->length = strlen(text);
    line->text = (char*)malloc(line->length + 1);
    strcpy(line->text, text);
    
    // Rebuild full text
    RebuildTextFromLines(area);
    UpdateTextMetrics(area);
    
    if (area->validator) {
        area->is_valid = area->validator(area->text);
    }
    
    if (area->on_text_changed) {
        area->on_text_changed(area->text, area->user_data);
    }
    
    if (area->on_line_changed) {
        area->on_line_changed(line_index, text, area->user_data);
    }
}

// State management functions
void TextArea_SetFocus(TextArea* area, bool focused) {
    if (!area || area->is_disabled) return;
    
    bool was_focused = area->is_focused;
    area->is_focused = focused;
    
    if (focused && !was_focused) {
        if (area->on_focus_gained) {
            area->on_focus_gained(area->user_data);
        }
    } else if (!focused && was_focused) {
        TextArea_ClearSelection(area);
        if (area->on_focus_lost) {
            area->on_focus_lost(area->user_data);
        }
    }
}

bool TextArea_IsFocused(TextArea* area) {
    return area ? area->is_focused : false;
}

void TextArea_SetDisabled(TextArea* area, bool disabled) {
    if (!area) return;
    
    area->is_disabled = disabled;
    if (disabled) {
        area->is_focused = false;
        area->is_hovered = false;
    }
}

void TextArea_SetReadonly(TextArea* area, bool readonly) {
    if (area) area->is_readonly = readonly;
}

void TextArea_SetVisible(TextArea* area, bool visible) {
    if (area) area->is_visible = visible;
}

// Selection and cursor functions
void TextArea_SelectAll(TextArea* area) {
    if (!area || area->lines.length == 0) return;
    
    area->selection_start_line = 0;
    area->selection_start_column = 0;
    area->selection_end_line = area->lines.length - 1;
    area->selection_end_column = area->lines.data[area->selection_end_line].length;
    
    area->cursor_line = area->selection_end_line;
    area->cursor_column = area->selection_end_column;
}

void TextArea_ClearSelection(TextArea* area) {
    if (!area) return;
    
    area->selection_start_line = area->cursor_line;
    area->selection_start_column = area->cursor_column;
    area->selection_end_line = area->cursor_line;
    area->selection_end_column = area->cursor_column;
}

void TextArea_SetCursorPosition(TextArea* area, int line, int column) {
    if (!area) return;
    
    area->cursor_line = line < 0 ? 0 : (line >= area->lines.length ? area->lines.length - 1 : line);
    
    if (area->cursor_line < area->lines.length) {
        int line_length = area->lines.data[area->cursor_line].length;
        area->cursor_column = column < 0 ? 0 : (column > line_length ? line_length : column);
    } else {
        area->cursor_column = 0;
    }
    
    TextArea_ClearSelection(area);
    ScrollToCursor(area);
}

void TextArea_GetCursorPosition(TextArea* area, int* line, int* column) {
    if (!area) {
        if (line) *line = 0;
        if (column) *column = 0;
        return;
    }
    
    if (line) *line = area->cursor_line;
    if (column) *column = area->cursor_column;
}

bool TextArea_HasSelection(TextArea* area) {
    if (!area) return false;
    
    return !(area->selection_start_line == area->selection_end_line && 
             area->selection_start_column == area->selection_end_column);
}

// Configuration functions
void TextArea_SetWordWrap(TextArea* area, bool word_wrap) {
    if (area) {
        area->word_wrap = word_wrap;
        UpdateTextMetrics(area);
    }
}

void TextArea_SetShowLineNumbers(TextArea* area, bool show) {
    if (area) {
        area->show_line_numbers = show;
        UpdateTextMetrics(area);
    }
}

void TextArea_SetAutoIndent(TextArea* area, bool auto_indent) {
    if (area) area->auto_indent = auto_indent;
}

void TextArea_SetTabSize(TextArea* area, int tab_size) {
    if (area && tab_size > 0) area->tab_size = tab_size;
}

// Visual customization functions
void TextArea_SetBorder(TextArea* area, bool show_border) {
    if (area) area->show_border = show_border;
}

void TextArea_SetShadow(TextArea* area, bool show_shadow) {
    if (area) area->show_shadow = show_shadow;
}

void TextArea_SetShowScrollBars(TextArea* area, bool show) {
    if (area) area->show_scroll_bars = show;
}

// Styling functions
void TextArea_SetBackgroundColor(TextArea* area, Color color) {
    if (!area) return;
    
    if (!area->custom_background_color) {
        area->custom_background_color = (Color*)malloc(sizeof(Color));
    }
    
    if (area->custom_background_color) {
        *area->custom_background_color = color;
    }
}

void TextArea_SetTextColor(TextArea* area, Color color) {
    if (!area) return;
    
    if (!area->custom_text_color) {
        area->custom_text_color = (Color*)malloc(sizeof(Color));
    }
    
    if (area->custom_text_color) {
        *area->custom_text_color = color;
    }
}

// Callback functions
void TextArea_SetOnTextChanged(TextArea* area, void (*callback)(const char*, void*), void* user_data) {
    if (!area) return;
    
    area->on_text_changed = callback;
    area->user_data = user_data;
}

void TextArea_SetOnFocusChanged(TextArea* area, 
                               void (*on_focus_gained)(void*), 
                               void (*on_focus_lost)(void*), 
                               void* user_data) {
    if (!area) return;
    
    area->on_focus_gained = on_focus_gained;
    area->on_focus_lost = on_focus_lost;
    area->user_data = user_data;
}

// Built-in validators
bool TextArea_ValidateNotEmpty(const char* text) {
    return text && strlen(text) > 0;
}

bool TextArea_ValidateMaxLines(const char* text, int max_lines) {
    if (!text) return true;
    
    int line_count = 1;
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            line_count++;
        }
    }
    
    return line_count <= max_lines;
}

// File operations
bool TextArea_LoadFromFile(TextArea* area, const char* filename) {
    if (!area || !filename) return false;
    
    FILE* file = fopen(filename, "r");
    if (!file) return false;
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return false;
    }
    
    // Allocate buffer
    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return false;
    }
    
    // Read file
    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    
    fclose(file);
    
    // Set text
    TextArea_SetText(area, buffer);
    free(buffer);
    
    return true;
}

bool TextArea_SaveToFile(TextArea* area, const char* filename) {
    if (!area || !filename || !area->text) return false;
    
    FILE* file = fopen(filename, "w");
    if (!file) return false;
    
    size_t bytes_written = fwrite(area->text, 1, strlen(area->text), file);
    fclose(file);
    
    return bytes_written == strlen(area->text);
}

// Utility functions
void TextArea_ResetCustomStyles(TextArea* area) {
    if (!area) return;
    
    // Free all custom styling
    if (area->custom_background_color) { free(area->custom_background_color); area->custom_background_color = NULL; }
    if (area->custom_text_color) { free(area->custom_text_color); area->custom_text_color = NULL; }
    if (area->custom_border_color) { free(area->custom_border_color); area->custom_border_color = NULL; }
    if (area->custom_cursor_color) { free(area->custom_cursor_color); area->custom_cursor_color = NULL; }
    if (area->custom_selection_color) { free(area->custom_selection_color); area->custom_selection_color = NULL; }
    if (area->custom_placeholder_color) { free(area->custom_placeholder_color); area->custom_placeholder_color = NULL; }
    if (area->custom_line_number_color) { free(area->custom_line_number_color); area->custom_line_number_color = NULL; }
    if (area->custom_scrollbar_color) { free(area->custom_scrollbar_color); area->custom_scrollbar_color = NULL; }
    if (area->custom_padding) { free(area->custom_padding); area->custom_padding = NULL; }
    if (area->custom_font_size) { free(area->custom_font_size); area->custom_font_size = NULL; }
    if (area->custom_line_height) { free(area->custom_line_height); area->custom_line_height = NULL; }
}

void TextArea_InsertText(TextArea* area, const char* text) {
    if (!area || !text || area->is_readonly || area->is_disabled) return;
    
    // Insert text character by character at cursor position
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n' || text[i] == '\r') {
            InsertNewLine(area);
        } else if (text[i] >= 32 && text[i] <= 126) {
            InsertCharacterAtCursor(area, text[i]);
        }
    }
}

void TextArea_DeleteSelection(TextArea* area) {
    if (!area || !TextArea_HasSelection(area) || area->is_readonly || area->is_disabled) return;
    
    // Get selection bounds
    int start_line = area->selection_start_line < area->selection_end_line ? area->selection_start_line : area->selection_end_line;
    int start_col = area->selection_start_line < area->selection_end_line ? area->selection_start_column : 
                   (area->selection_start_line == area->selection_end_line && area->selection_start_column < area->selection_end_column) ? 
                   area->selection_start_column : area->selection_end_column;
    
    int end_line = area->selection_start_line > area->selection_end_line ? area->selection_start_line : area->selection_end_line;
    int end_col = area->selection_start_line > area->selection_end_line ? area->selection_start_column : 
                 (area->selection_start_line == area->selection_end_line && area->selection_start_column > area->selection_end_column) ? 
                 area->selection_start_column : area->selection_end_column;
    
    if (start_line == end_line) {
        // Single line selection
        TextLine* line = &area->lines.data[start_line];
        memmove(line->text + start_col, line->text + end_col, line->length - end_col + 1);
        line->length -= (end_col - start_col);
        line->text = (char*)realloc(line->text, line->length + 1);
    } else {
        // Multi-line selection
        TextLine* start_line_ptr = &area->lines.data[start_line];
        TextLine* end_line_ptr = &area->lines.data[end_line];
        
        // Combine start and end lines
        int remaining_start = start_col;
        int remaining_end = end_line_ptr->length - end_col;
        
        char* new_text = (char*)malloc(remaining_start + remaining_end + 1);
        strncpy(new_text, start_line_ptr->text, remaining_start);
        strcpy(new_text + remaining_start, end_line_ptr->text + end_col);
        
        free(start_line_ptr->text);
        start_line_ptr->text = new_text;
        start_line_ptr->length = remaining_start + remaining_end;
        
        // Remove lines in between
        for (int i = start_line + 1; i <= end_line; i++) {
            free(area->lines.data[start_line + 1].text);
            vec_splice(&area->lines, start_line + 1, 1);
        }
    }
    
    // Set cursor position and clear selection
    area->cursor_line = start_line;
    area->cursor_column = start_col;
    TextArea_ClearSelection(area);
    
    // Rebuild and update
    RebuildTextFromLines(area);
    UpdateTextMetrics(area);
    ScrollToCursor(area);
    
    if (area->validator) {
        area->is_valid = area->validator(area->text);
    }
    
    if (area->on_text_changed) {
        area->on_text_changed(area->text, area->user_data);
    }
}