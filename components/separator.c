#include "separator.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper function to copy string
static char* copy_string(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = malloc(len + 1);
    if (copy) {
        strcpy(copy, str);
    }
    return copy;
}

// Creation and destruction
Separator Separator_Create(Rectangle bounds, SeparatorType type) {
    Separator separator = {0};
    separator.bounds = bounds;
    separator.type = type;
    separator.style = SEPARATOR_SOLID;
    separator.color = LIGHTGRAY;
    separator.thickness = 1.0f;
    separator.opacity = 1.0f;
    separator.visible = true;
    separator.text = NULL;
    separator.text_align = SEPARATOR_TEXT_CENTER;
    separator.text_color = DARKGRAY;
    separator.font_size = 12;
    separator.text_padding = 10.0f;
    separator.dash_length = 5.0f;
    separator.gap_length = 3.0f;
    separator.fade_in_duration = 0.0f;
    separator.current_fade_time = 0.0f;
    separator.is_animating = false;
    separator.z_index = 0;
    return separator;
}

Separator Separator_CreateHorizontal(float x, float y, float width) {
    return Separator_Create((Rectangle){x, y, width, 1}, SEPARATOR_HORIZONTAL);
}

Separator Separator_CreateVertical(float x, float y, float height) {
    return Separator_Create((Rectangle){x, y, 1, height}, SEPARATOR_VERTICAL);
}

Separator Separator_CreateSpacer(float x, float y, float width, float height) {
    Separator separator = Separator_Create((Rectangle){x, y, width, height}, SEPARATOR_SPACER);
    separator.visible = false; // Spacers are invisible by default
    return separator;
}

Separator Separator_CreateWithText(Rectangle bounds, const char* text, SeparatorTextAlign align) {
    Separator separator = Separator_Create(bounds, SEPARATOR_HORIZONTAL);
    Separator_SetText(&separator, text);
    separator.text_align = align;
    return separator;
}

void Separator_Destroy(Separator* separator) {
    if (!separator) return;
    if (separator->text) {
        free(separator->text);
        separator->text = NULL;
    }
}

// Basic properties
void Separator_SetType(Separator* separator, SeparatorType type) {
    if (!separator) return;
    separator->type = type;
}

void Separator_SetStyle(Separator* separator, SeparatorStyle style) {
    if (!separator) return;
    separator->style = style;
}

void Separator_SetColor(Separator* separator, Color color) {
    if (!separator) return;
    separator->color = color;
}

void Separator_SetThickness(Separator* separator, float thickness) {
    if (!separator) return;
    separator->thickness = fmaxf(0.1f, thickness);
}

void Separator_SetOpacity(Separator* separator, float opacity) {
    if (!separator) return;
    separator->opacity = fmaxf(0.0f, fminf(1.0f, opacity));
}

void Separator_SetVisible(Separator* separator, bool visible) {
    if (!separator) return;
    separator->visible = visible;
}

// Margins
void Separator_SetMargin(Separator* separator, float top, float right, float bottom, float left) {
    if (!separator) return;
    separator->margin_top = fmaxf(0.0f, top);
    separator->margin_right = fmaxf(0.0f, right);
    separator->margin_bottom = fmaxf(0.0f, bottom);
    separator->margin_left = fmaxf(0.0f, left);
}

void Separator_SetMarginHorizontal(Separator* separator, float horizontal) {
    Separator_SetMargin(separator, separator->margin_top, horizontal, separator->margin_bottom, horizontal);
}

void Separator_SetMarginVertical(Separator* separator, float vertical) {
    Separator_SetMargin(separator, vertical, separator->margin_right, vertical, separator->margin_left);
}

void Separator_SetMarginAll(Separator* separator, float margin) {
    Separator_SetMargin(separator, margin, margin, margin, margin);
}

// Text label
void Separator_SetText(Separator* separator, const char* text) {
    if (!separator) return;
    if (separator->text) {
        free(separator->text);
    }
    separator->text = copy_string(text);
}

void Separator_SetTextAlign(Separator* separator, SeparatorTextAlign align) {
    if (!separator) return;
    separator->text_align = align;
}

void Separator_SetTextColor(Separator* separator, Color color) {
    if (!separator) return;
    separator->text_color = color;
}

void Separator_SetFontSize(Separator* separator, int font_size) {
    if (!separator) return;
    separator->font_size = font_size > 0 ? font_size : 12;
}

void Separator_SetTextPadding(Separator* separator, float padding) {
    if (!separator) return;
    separator->text_padding = fmaxf(0.0f, padding);
}

void Separator_ClearText(Separator* separator) {
    if (!separator) return;
    if (separator->text) {
        free(separator->text);
        separator->text = NULL;
    }
}

// Dashed/dotted styling
void Separator_SetDashPattern(Separator* separator, float dash_length, float gap_length) {
    if (!separator) return;
    separator->dash_length = fmaxf(1.0f, dash_length);
    separator->gap_length = fmaxf(1.0f, gap_length);
}

// Animation
void Separator_SetFadeIn(Separator* separator, float duration) {
    if (!separator) return;
    separator->fade_in_duration = fmaxf(0.0f, duration);
}

void Separator_StartFadeIn(Separator* separator) {
    if (!separator) return;
    separator->current_fade_time = 0.0f;
    separator->is_animating = separator->fade_in_duration > 0.0f;
}

// Layout
void Separator_SetBounds(Separator* separator, Rectangle bounds) {
    if (!separator) return;
    separator->bounds = bounds;
}

void Separator_SetPosition(Separator* separator, float x, float y) {
    if (!separator) return;
    separator->bounds.x = x;
    separator->bounds.y = y;
}

void Separator_SetSize(Separator* separator, float width, float height) {
    if (!separator) return;
    separator->bounds.width = fmaxf(0.0f, width);
    separator->bounds.height = fmaxf(0.0f, height);
}

void Separator_SetZIndex(Separator* separator, int z_index) {
    if (!separator) return;
    separator->z_index = z_index;
}

// Utility functions
Rectangle Separator_GetBounds(const Separator* separator) {
    if (!separator) return (Rectangle){0};
    return separator->bounds;
}

Rectangle Separator_GetContentBounds(const Separator* separator) {
    if (!separator) return (Rectangle){0};
    
    Rectangle content = separator->bounds;
    content.x += separator->margin_left;
    content.y += separator->margin_top;
    content.width -= (separator->margin_left + separator->margin_right);
    content.height -= (separator->margin_top + separator->margin_bottom);
    
    // Ensure non-negative dimensions
    content.width = fmaxf(0.0f, content.width);
    content.height = fmaxf(0.0f, content.height);
    
    return content;
}

bool Separator_HasText(const Separator* separator) {
    return separator && separator->text && strlen(separator->text) > 0;
}

float Separator_GetTextWidth(const Separator* separator) {
    if (!Separator_HasText(separator)) return 0.0f;
    return MeasureText(separator->text, separator->font_size);
}

// Update and drawing
void Separator_Update(Separator* separator) {
    if (!separator) return;
    
    // Update fade-in animation
    if (separator->is_animating && separator->fade_in_duration > 0.0f) {
        separator->current_fade_time += GetFrameTime();
        if (separator->current_fade_time >= separator->fade_in_duration) {
            separator->current_fade_time = separator->fade_in_duration;
            separator->is_animating = false;
        }
    }
}

static void DrawDashedLine(Vector2 start, Vector2 end, float thickness, Color color, float dash_length, float gap_length) {
    Vector2 direction = {end.x - start.x, end.y - start.y};
    float total_length = sqrtf(direction.x * direction.x + direction.y * direction.y);
    
    if (total_length <= 0.0f) return;
    
    // Normalize direction
    direction.x /= total_length;
    direction.y /= total_length;
    
    float current_pos = 0.0f;
    bool drawing_dash = true;
    
    while (current_pos < total_length) {
        float segment_length = drawing_dash ? dash_length : gap_length;
        float end_pos = fminf(current_pos + segment_length, total_length);
        
        if (drawing_dash) {
            Vector2 dash_start = {
                start.x + direction.x * current_pos,
                start.y + direction.y * current_pos
            };
            Vector2 dash_end = {
                start.x + direction.x * end_pos,
                start.y + direction.y * end_pos
            };
            
            if (thickness <= 1.0f) {
                DrawLineV(dash_start, dash_end, color);
            } else {
                DrawLineEx(dash_start, dash_end, thickness, color);
            }
        }
        
        current_pos = end_pos;
        drawing_dash = !drawing_dash;
    }
}

static void DrawDottedLine(Vector2 start, Vector2 end, float thickness, Color color, float gap_length) {
    Vector2 direction = {end.x - start.x, end.y - start.y};
    float total_length = sqrtf(direction.x * direction.x + direction.y * direction.y);
    
    if (total_length <= 0.0f) return;
    
    // Normalize direction
    direction.x /= total_length;
    direction.y /= total_length;
    
    float dot_spacing = gap_length;
    int dot_count = (int)(total_length / dot_spacing) + 1;
    
    for (int i = 0; i < dot_count; i++) {
        float pos = i * dot_spacing;
        if (pos > total_length) break;
        
        Vector2 dot_pos = {
            start.x + direction.x * pos,
            start.y + direction.y * pos
        };
        
        float radius = thickness * 0.5f;
        if (radius < 1.0f) {
            DrawPixelV(dot_pos, color);
        } else {
            DrawCircleV(dot_pos, radius, color);
        }
    }
}

void Separator_Draw(const Separator* separator) {
    if (!separator || !separator->visible) return;
    
    Rectangle content_bounds = Separator_GetContentBounds(separator);
    if (content_bounds.width <= 0 || content_bounds.height <= 0) return;
    
    // Calculate opacity with fade-in animation
    float current_opacity = separator->opacity;
    if (separator->is_animating && separator->fade_in_duration > 0.0f) {
        float fade_progress = separator->current_fade_time / separator->fade_in_duration;
        current_opacity *= fade_progress;
    }
    
    Color draw_color = separator->color;
    draw_color.a = (unsigned char)(draw_color.a * current_opacity);
    
    // Don't draw if completely transparent
    if (draw_color.a == 0) return;
    
    // Handle spacer type (invisible)
    if (separator->type == SEPARATOR_SPACER) return;
    
    // Draw text label if present
    if (Separator_HasText(separator) && separator->type == SEPARATOR_HORIZONTAL) {
        float text_width = Separator_GetTextWidth(separator);
        float text_x = content_bounds.x;
        
        // Calculate text position based on alignment
        switch (separator->text_align) {
            case SEPARATOR_TEXT_LEFT:
                text_x = content_bounds.x;
                break;
            case SEPARATOR_TEXT_CENTER:
                text_x = content_bounds.x + (content_bounds.width - text_width) * 0.5f;
                break;
            case SEPARATOR_TEXT_RIGHT:
                text_x = content_bounds.x + content_bounds.width - text_width;
                break;
        }
        
        float text_y = content_bounds.y + (content_bounds.height - separator->font_size) * 0.5f;
        
        // Draw text background (optional clear area)
        Rectangle text_bg = {
            text_x - separator->text_padding,
            text_y - 2,
            text_width + separator->text_padding * 2,
            separator->font_size + 4
        };
        
        Color text_color = separator->text_color;
        text_color.a = (unsigned char)(text_color.a * current_opacity);
        
        DrawText(separator->text, (int)text_x, (int)text_y, separator->font_size, text_color);
        
        // Draw line segments around text
        float line_y = content_bounds.y + content_bounds.height * 0.5f;
        
        // Left line segment
        if (text_x > content_bounds.x + separator->text_padding) {
            Vector2 left_start = {content_bounds.x, line_y};
            Vector2 left_end = {text_x - separator->text_padding, line_y};
            
            switch (separator->style) {
                case SEPARATOR_SOLID:
                    if (separator->thickness <= 1.0f) {
                        DrawLineV(left_start, left_end, draw_color);
                    } else {
                        DrawLineEx(left_start, left_end, separator->thickness, draw_color);
                    }
                    break;
                case SEPARATOR_DASHED:
                    DrawDashedLine(left_start, left_end, separator->thickness, draw_color, 
                                 separator->dash_length, separator->gap_length);
                    break;
                case SEPARATOR_DOTTED:
                    DrawDottedLine(left_start, left_end, separator->thickness, draw_color, separator->gap_length);
                    break;
            }
        }
        
        // Right line segment
        float right_start_x = text_x + text_width + separator->text_padding;
        if (right_start_x < content_bounds.x + content_bounds.width) {
            Vector2 right_start = {right_start_x, line_y};
            Vector2 right_end = {content_bounds.x + content_bounds.width, line_y};
            
            switch (separator->style) {
                case SEPARATOR_SOLID:
                    if (separator->thickness <= 1.0f) {
                        DrawLineV(right_start, right_end, draw_color);
                    } else {
                        DrawLineEx(right_start, right_end, separator->thickness, draw_color);
                    }
                    break;
                case SEPARATOR_DASHED:
                    DrawDashedLine(right_start, right_end, separator->thickness, draw_color, 
                                 separator->dash_length, separator->gap_length);
                    break;
                case SEPARATOR_DOTTED:
                    DrawDottedLine(right_start, right_end, separator->thickness, draw_color, separator->gap_length);
                    break;
            }
        }
    } else {
        // Draw simple line without text
        Vector2 start, end;
        
        if (separator->type == SEPARATOR_HORIZONTAL) {
            float line_y = content_bounds.y + content_bounds.height * 0.5f;
            start = (Vector2){content_bounds.x, line_y};
            end = (Vector2){content_bounds.x + content_bounds.width, line_y};
        } else { // SEPARATOR_VERTICAL
            float line_x = content_bounds.x + content_bounds.width * 0.5f;
            start = (Vector2){line_x, content_bounds.y};
            end = (Vector2){line_x, content_bounds.y + content_bounds.height};
        }
        
        switch (separator->style) {
            case SEPARATOR_SOLID:
                if (separator->thickness <= 1.0f) {
                    DrawLineV(start, end, draw_color);
                } else {
                    DrawLineEx(start, end, separator->thickness, draw_color);
                }
                break;
            case SEPARATOR_DASHED:
                DrawDashedLine(start, end, separator->thickness, draw_color, 
                             separator->dash_length, separator->gap_length);
                break;
            case SEPARATOR_DOTTED:
                DrawDottedLine(start, end, separator->thickness, draw_color, separator->gap_length);
                break;
        }
    }
}

// Preset creators for common use cases
Separator Separator_CreateMenuDivider(float x, float y, float width) {
    Separator separator = Separator_CreateHorizontal(x, y, width);
    separator.color = GRAY;
    separator.thickness = 1.0f;
    Separator_SetMarginVertical(&separator, 5.0f);
    return separator;
}

Separator Separator_CreateFormSection(float x, float y, float width, const char* section_title) {
    Separator separator = Separator_CreateWithText((Rectangle){x, y, width, 20}, section_title, SEPARATOR_TEXT_LEFT);
    separator.color = LIGHTGRAY;
    separator.text_color = DARKGRAY;
    separator.font_size = 14;
    Separator_SetMarginVertical(&separator, 15.0f);
    return separator;
}

Separator Separator_CreateToolbarDivider(float x, float y, float height) {
    Separator separator = Separator_CreateVertical(x, y, height);
    separator.color = GRAY;
    separator.thickness = 1.0f;
    Separator_SetMarginHorizontal(&separator, 8.0f);
    return separator;
}

Separator Separator_CreateContentBreak(float x, float y, float width) {
    Separator separator = Separator_CreateHorizontal(x, y, width);
    separator.style = SEPARATOR_DASHED;
    separator.color = LIGHTGRAY;
    separator.thickness = 1.0f;
    separator.dash_length = 8.0f;
    separator.gap_length = 4.0f;
    Separator_SetMarginVertical(&separator, 20.0f);
    return separator;
}