#include "button.h"
#include "theme.h"
#include "shadow.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

Button Button_Create(Rectangle rect, const char* text, ButtonVariant variant) {
    Button button = {0};
    
    // Set rectangle bounds
    button.rect = rect;
    
    // Handle text - create a copy to avoid external pointer dependencies
    if (text != NULL) {
        size_t text_len = strlen(text);
        button.text = (char*)malloc(text_len + 1);
        if (button.text != NULL) {
            strcpy(button.text, text);
        }
    } else {
        // Use empty string if text is NULL
        button.text = (char*)malloc(1);
        if (button.text != NULL) {
            button.text[0] = '\0';
        }
    }
    
    // Set variant
    button.variant = variant;
    
    // Initialize interaction states
    button.is_hovered = false;
    button.is_pressed = false;
    button.is_clicked = false;
    button.is_focused = false;
    button.is_disabled = false;
    button.is_visible = true;
    
    // Initialize all custom style pointers to NULL (use theme defaults)
    button.custom_padding = NULL;
    button.custom_margin = NULL;
    button.custom_border_radius = NULL;
    button.custom_background_color = NULL;
    button.custom_text_color = NULL;
    button.custom_border_color = NULL;
    button.custom_font_size = NULL;
    button.custom_text_align = NULL;
    button.custom_text_vertical_align = NULL;
    button.custom_opacity = NULL;
    button.custom_shadow_offset = NULL;
    button.custom_shadow_blur = NULL;
    
    // Initialize visual state flags
    button.show_border = true;
    button.show_outline = false;
    button.show_shadow = false;
    
    // Initialize layout and positioning
    button.z_index = 10; // Default from theme
    button.transform_offset = (Vector2){0.0f, 0.0f};
    button.rotation = 0.0f;
    button.scale = (Vector2){1.0f, 1.0f};
    
    return button;
}

void Button_Update(Button* button) {
    if (!button || !button->is_visible || button->is_disabled) {
        return;
    }
    
    Vector2 mouse_pos = GetMousePosition();
    
    // Apply transform to mouse position for hit testing
    Vector2 transformed_mouse = mouse_pos;
    transformed_mouse.x -= button->transform_offset.x;
    transformed_mouse.y -= button->transform_offset.y;
    
    // Check if mouse is within button bounds
    bool mouse_over = CheckCollisionPointRec(transformed_mouse, button->rect);
    
    // Update hover state
    button->is_hovered = mouse_over;
    
    // Update pressed state
    if (mouse_over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        button->is_pressed = true;
    }
    
    // Update clicked state and reset pressed state
    button->is_clicked = false;
    if (button->is_pressed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (mouse_over) {
            button->is_clicked = true;
        }
        button->is_pressed = false;
    }
    
    // Handle focus (basic keyboard navigation)
    if (IsKeyPressed(KEY_TAB)) {
        // Simple focus toggle for demonstration
        button->is_focused = !button->is_focused;
    }
    
    // Auto-enable outline when focused
    if (button->is_focused) {
        button->show_outline = true;
    } else if (!button->is_hovered) {
        button->show_outline = false;
    }
}

void Button_Draw(const Button* button) {
    if (!button || !button->is_visible) {
        return;
    }

    // Determine the component's current state
    ComponentState state = STATE_DEFAULT;
    if (button->is_disabled) {
        state = STATE_DISABLED;
    } else if (button->is_pressed) {
        state = STATE_PRESSED;
    } else if (button->is_hovered) {
        state = STATE_HOVER;
    }

    // Get the base style for the button variant
    ComponentStyle* style;
    switch (button->variant) {
        case BUTTON_PRIMARY:
            style = &AppTheme.primary_button;
            break;
        case BUTTON_SECONDARY:
            style = &AppTheme.secondary_button;
            break;
        case BUTTON_DESTRUCTIVE:
            style = &AppTheme.destructive_button;
            break;
        default:
            style = &AppTheme.primary_button;
            break;
    }

    // Get the colors for the current state
    ColorPalette* colors = &style->colors[state];

    // Get effective values (custom overrides or theme defaults)
    EdgeValues padding = button->custom_padding ? *button->custom_padding : style->padding;
    EdgeValues margin = button->custom_margin ? *button->custom_margin : style->margin;
    CornerRadius border_radius = button->custom_border_radius ? *button->custom_border_radius : style->border_radius;
    float opacity = button->custom_opacity ? *button->custom_opacity : style->opacity;
    
    // Get colors from the palette
    Color bg_color = colors->background;
    Color text_color = colors->text;
    Color border_color = colors->border;

    // Apply custom color overrides
    if (button->custom_background_color) bg_color = *button->custom_background_color;
    if (button->custom_text_color) text_color = *button->custom_text_color;
    if (button->custom_border_color) border_color = *button->custom_border_color;
    
    // Apply opacity
    bg_color.a = (unsigned char)(bg_color.a * opacity);
    text_color.a = (unsigned char)(text_color.a * opacity);
    border_color.a = (unsigned char)(border_color.a * opacity);

    // Calculate render rectangle (accounting for margin and transforms)
    Rectangle render_rect = button->rect;
    render_rect.x += margin.left + button->transform_offset.x;
    render_rect.y += margin.top + button->transform_offset.y;
    render_rect.width -= (margin.left + margin.right);
    render_rect.height -= (margin.top + margin.bottom);
    
    // Apply scaling
    render_rect.width *= button->scale.x;
    render_rect.height *= button->scale.y;

    // Draw shadow (if enabled)
    if (button->show_shadow) {
        ShadowConfig shadow_config = Shadow_FromTheme(style, state);
        shadow_config.opacity *= opacity; // Apply button opacity
        
        if (button->custom_shadow_offset) shadow_config.offset = *button->custom_shadow_offset;
        if (button->custom_shadow_blur) shadow_config.blur_radius = *button->custom_shadow_blur;
        
        Shadow_DrawRectangleRounded(render_rect, border_radius.top_left, &shadow_config);
    }

    // Calculate roundness for consistent shape rendering
    float min_dimension = fminf(render_rect.width, render_rect.height);
    bool is_circular = border_radius.top_left >= min_dimension / 2.0f;
    float roundness = is_circular ? 1.0f : (border_radius.top_left / (render_rect.height / 2.0f));
    roundness = fminf(roundness, 1.0f); // Clamp to max roundness

    // Draw background
    if (is_circular && render_rect.width == render_rect.height) {
        // Draw circular background for square buttons with high border radius
        Vector2 center = {
            render_rect.x + render_rect.width / 2.0f,
            render_rect.y + render_rect.height / 2.0f
        };
        float radius = min_dimension / 2.0f;
        DrawCircle((int)center.x, (int)center.y, radius, bg_color);
    } else {
        // Draw rounded rectangle background
        DrawRectangleRounded(render_rect, roundness, 10, bg_color);
    }

    // Draw border
    if (button->show_border) {
        if (is_circular && render_rect.width == render_rect.height) {
            // Draw circular border for square buttons with high border radius
            Vector2 center = {
                render_rect.x + render_rect.width / 2.0f,
                render_rect.y + render_rect.height / 2.0f
            };
            float radius = min_dimension / 2.0f;
            
            // Draw border with proper thickness
            DrawCircleLines((int)center.x, (int)center.y, radius, border_color);
            if (style->border_width.top > 1.0f) {
                // Draw multiple circles for thicker borders
                for (int i = 1; i < (int)style->border_width.top; i++) {
                    DrawCircleLines((int)center.x, (int)center.y, radius - i, border_color);
                }
            }
        } else {
            // Draw rounded rectangle border with matching roundness
            DrawRectangleRoundedLines(render_rect, roundness, 10, border_color);
        }
    }

    // Draw outline
    if (button->show_outline) {
        Rectangle outline_rect = render_rect;
        outline_rect.x -= style->outline_width.left;
        outline_rect.y -= style->outline_width.top;
        outline_rect.width += (style->outline_width.left + style->outline_width.right);
        outline_rect.height += (style->outline_width.top + style->outline_width.bottom);
        
        Color outline_color = style->colors[STATE_ACTIVE].accent; // Use accent color for outline
        outline_color.a = (unsigned char)(outline_color.a * opacity);
        
        // Calculate outline roundness to match the button shape
        float outline_min_dimension = fminf(outline_rect.width, outline_rect.height);
        bool outline_is_circular = border_radius.top_left >= outline_min_dimension / 2.0f;
        
        if (outline_is_circular && outline_rect.width == outline_rect.height) {
            // Draw circular outline
            Vector2 outline_center = {
                outline_rect.x + outline_rect.width / 2.0f,
                outline_rect.y + outline_rect.height / 2.0f
            };
            float outline_radius = outline_min_dimension / 2.0f;
            DrawCircleLines((int)outline_center.x, (int)outline_center.y, outline_radius, outline_color);
        } else {
            // Draw rounded rectangle outline with matching roundness
            float outline_roundness = outline_is_circular ? 1.0f : (border_radius.top_left / (outline_rect.height / 2.0f));
            outline_roundness = fminf(outline_roundness, 1.0f);
            DrawRectangleRoundedLines(outline_rect, outline_roundness, 10, outline_color);
        }
    }

    // Draw text
    if (button->text && strlen(button->text) > 0) {
        int font_size = button->custom_font_size ? *button->custom_font_size : style->font_size;
        TextAlign h_align = button->custom_text_align ? *button->custom_text_align : style->text_align;
        TextVerticalAlign v_align = button->custom_text_vertical_align ? *button->custom_text_vertical_align : style->text_vertical_align;
        
        Rectangle text_area = render_rect;
        text_area.x += padding.left;
        text_area.y += padding.top;
        text_area.width -= (padding.left + padding.right);
        text_area.height -= (padding.top + padding.bottom);
        
        Vector2 text_size = MeasureTextEx(GetFontDefault(), button->text, font_size, style->letter_spacing);
        Vector2 text_pos = {text_area.x, text_area.y};
        
        switch (h_align) {
            case TEXT_ALIGN_CENTER: text_pos.x += (text_area.width - text_size.x) / 2.0f; break;
            case TEXT_ALIGN_RIGHT: text_pos.x += text_area.width - text_size.x; break;
            default: break;
        }
        
        switch (v_align) {
            case TEXT_VERTICAL_CENTER: text_pos.y += (text_area.height - text_size.y) / 2.0f; break;
            case TEXT_VERTICAL_BOTTOM: text_pos.y += text_area.height - text_size.y; break;
            default: break;
        }
        
        DrawTextEx(GetFontDefault(), button->text, text_pos, font_size, style->letter_spacing, text_color);
    }
}

void Button_Destroy(Button* button) {
    if (!button) return;
    
    // Free text memory
    if (button->text) {
        free(button->text);
        button->text = NULL;
    }
    
    // Free custom style allocations
    if (button->custom_padding) { free(button->custom_padding); button->custom_padding = NULL; }
    if (button->custom_margin) { free(button->custom_margin); button->custom_margin = NULL; }
    if (button->custom_border_radius) { free(button->custom_border_radius); button->custom_border_radius = NULL; }
    if (button->custom_background_color) { free(button->custom_background_color); button->custom_background_color = NULL; }
    if (button->custom_text_color) { free(button->custom_text_color); button->custom_text_color = NULL; }
    if (button->custom_border_color) { free(button->custom_border_color); button->custom_border_color = NULL; }
    if (button->custom_font_size) { free(button->custom_font_size); button->custom_font_size = NULL; }
    if (button->custom_text_align) { free(button->custom_text_align); button->custom_text_align = NULL; }
    if (button->custom_text_vertical_align) { free(button->custom_text_vertical_align); button->custom_text_vertical_align = NULL; }
    if (button->custom_opacity) { free(button->custom_opacity); button->custom_opacity = NULL; }
    if (button->custom_shadow_offset) { free(button->custom_shadow_offset); button->custom_shadow_offset = NULL; }
    if (button->custom_shadow_blur) { free(button->custom_shadow_blur); button->custom_shadow_blur = NULL; }
}
// Visual State Functions
void Button_SetBorder(Button* button, bool show_border) {
    if (button) button->show_border = show_border;
}

void Button_SetOutline(Button* button, bool show_outline) {
    if (button) button->show_outline = show_outline;
}

void Button_SetShadow(Button* button, bool show_shadow) {
    if (button) button->show_shadow = show_shadow;
}

void Button_SetVisible(Button* button, bool is_visible) {
    if (button) button->is_visible = is_visible;
}

void Button_SetDisabled(Button* button, bool is_disabled) {
    if (button) button->is_disabled = is_disabled;
}

// CSS-like Styling Functions
void Button_SetPadding(Button* button, float top, float right, float bottom, float left) {
    if (!button) return;
    
    if (!button->custom_padding) {
        button->custom_padding = (EdgeValues*)malloc(sizeof(EdgeValues));
    }
    
    if (button->custom_padding) {
        button->custom_padding->top = top;
        button->custom_padding->right = right;
        button->custom_padding->bottom = bottom;
        button->custom_padding->left = left;
    }
}

void Button_SetMargin(Button* button, float top, float right, float bottom, float left) {
    if (!button) return;
    
    if (!button->custom_margin) {
        button->custom_margin = (EdgeValues*)malloc(sizeof(EdgeValues));
    }
    
    if (button->custom_margin) {
        button->custom_margin->top = top;
        button->custom_margin->right = right;
        button->custom_margin->bottom = bottom;
        button->custom_margin->left = left;
    }
}

void Button_SetBorderRadius(Button* button, float top_left, float top_right, float bottom_right, float bottom_left) {
    if (!button) return;
    
    if (!button->custom_border_radius) {
        button->custom_border_radius = (CornerRadius*)malloc(sizeof(CornerRadius));
    }
    
    if (button->custom_border_radius) {
        button->custom_border_radius->top_left = top_left;
        button->custom_border_radius->top_right = top_right;
        button->custom_border_radius->bottom_right = bottom_right;
        button->custom_border_radius->bottom_left = bottom_left;
    }
}

void Button_SetBackgroundColor(Button* button, Color color) {
    if (!button) return;
    
    if (!button->custom_background_color) {
        button->custom_background_color = (Color*)malloc(sizeof(Color));
    }
    
    if (button->custom_background_color) {
        *button->custom_background_color = color;
    }
}

void Button_SetTextColor(Button* button, Color color) {
    if (!button) return;
    
    if (!button->custom_text_color) {
        button->custom_text_color = (Color*)malloc(sizeof(Color));
    }
    
    if (button->custom_text_color) {
        *button->custom_text_color = color;
    }
}

void Button_SetBorderColor(Button* button, Color color) {
    if (!button) return;
    
    if (!button->custom_border_color) {
        button->custom_border_color = (Color*)malloc(sizeof(Color));
    }
    
    if (button->custom_border_color) {
        *button->custom_border_color = color;
    }
}

void Button_SetFontSize(Button* button, int font_size) {
    if (!button) return;
    
    if (!button->custom_font_size) {
        button->custom_font_size = (int*)malloc(sizeof(int));
    }
    
    if (button->custom_font_size) {
        *button->custom_font_size = font_size;
    }
}

void Button_SetTextAlign(Button* button, TextAlign horizontal, TextVerticalAlign vertical) {
    if (!button) return;
    
    if (!button->custom_text_align) {
        button->custom_text_align = (TextAlign*)malloc(sizeof(TextAlign));
    }
    if (!button->custom_text_vertical_align) {
        button->custom_text_vertical_align = (TextVerticalAlign*)malloc(sizeof(TextVerticalAlign));
    }
    
    if (button->custom_text_align) {
        *button->custom_text_align = horizontal;
    }
    if (button->custom_text_vertical_align) {
        *button->custom_text_vertical_align = vertical;
    }
}

void Button_SetOpacity(Button* button, float opacity) {
    if (!button) return;
    
    if (!button->custom_opacity) {
        button->custom_opacity = (float*)malloc(sizeof(float));
    }
    
    if (button->custom_opacity) {
        *button->custom_opacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
    }
}

void Button_SetShadowStyle(Button* button, Vector2 offset, float blur, float spread, Color color) {
    if (!button) return;
    
    if (!button->custom_shadow_offset) {
        button->custom_shadow_offset = (Vector2*)malloc(sizeof(Vector2));
    }
    if (!button->custom_shadow_blur) {
        button->custom_shadow_blur = (float*)malloc(sizeof(float));
    }
    
    if (button->custom_shadow_offset) {
        *button->custom_shadow_offset = offset;
    }
    if (button->custom_shadow_blur) {
        *button->custom_shadow_blur = blur;
    }
    
    // Note: spread and color would need additional custom fields in full implementation
}

void Button_SetTransform(Button* button, Vector2 offset, float rotation, Vector2 scale) {
    if (!button) return;
    
    button->transform_offset = offset;
    button->rotation = rotation;
    button->scale = scale;
}

void Button_SetZIndex(Button* button, int z_index) {
    if (button) button->z_index = z_index;
}

// CSS Reset Functions
void Button_ResetPadding(Button* button) {
    if (!button) return;
    
    if (button->custom_padding) {
        free(button->custom_padding);
        button->custom_padding = NULL;
    }
}

void Button_ResetMargin(Button* button) {
    if (!button) return;
    
    if (button->custom_margin) {
        free(button->custom_margin);
        button->custom_margin = NULL;
    }
}

void Button_ResetAllCustomStyles(Button* button) {
    if (!button) return;
    
    // Free all custom style allocations
    if (button->custom_padding) { free(button->custom_padding); button->custom_padding = NULL; }
    if (button->custom_margin) { free(button->custom_margin); button->custom_margin = NULL; }
    if (button->custom_border_radius) { free(button->custom_border_radius); button->custom_border_radius = NULL; }
    if (button->custom_background_color) { free(button->custom_background_color); button->custom_background_color = NULL; }
    if (button->custom_text_color) { free(button->custom_text_color); button->custom_text_color = NULL; }
    if (button->custom_border_color) { free(button->custom_border_color); button->custom_border_color = NULL; }
    if (button->custom_font_size) { free(button->custom_font_size); button->custom_font_size = NULL; }
    if (button->custom_text_align) { free(button->custom_text_align); button->custom_text_align = NULL; }
    if (button->custom_text_vertical_align) { free(button->custom_text_vertical_align); button->custom_text_vertical_align = NULL; }
    if (button->custom_opacity) { free(button->custom_opacity); button->custom_opacity = NULL; }
    if (button->custom_shadow_offset) { free(button->custom_shadow_offset); button->custom_shadow_offset = NULL; }
    if (button->custom_shadow_blur) { free(button->custom_shadow_blur); button->custom_shadow_blur = NULL; }
    
    // Reset transform and positioning to defaults
    button->transform_offset = (Vector2){0.0f, 0.0f};
    button->rotation = 0.0f;
    button->scale = (Vector2){1.0f, 1.0f};
    button->z_index = 10; // Default z-index
}

bool Button_IsClicked(const Button* button) {
    return button ? button->is_clicked : false;
}