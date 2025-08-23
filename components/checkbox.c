#include "checkbox.h"
#include "shadow.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper function to get effective style values
static ComponentStyle* GetEffectiveStyle(Checkbox* checkbox) {
    return Theme_GetComponentStyle(COMPONENT_CHECKBOX);
}

static ComponentState GetCurrentState(Checkbox* checkbox) {
    if (checkbox->is_disabled) return STATE_DISABLED;
    if (checkbox->is_checked) return STATE_ACTIVE;
    if (checkbox->is_pressed) return STATE_PRESSED;
    if (checkbox->is_hovered) return STATE_HOVER;
    return STATE_DEFAULT;
}

// Helper function for smooth animation easing
static float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

Checkbox Checkbox_Create(Rectangle bounds, const char* label, bool initial_checked) {
    Checkbox checkbox = {0};
    
    checkbox.bounds = bounds;
    checkbox.is_checked = initial_checked;
    
    // Set label
    if (label) {
        size_t label_len = strlen(label) + 1;
        checkbox.label = (char*)malloc(label_len);
        strcpy(checkbox.label, label);
    }
    
    // Initialize state
    checkbox.is_hovered = false;
    checkbox.is_pressed = false;
    checkbox.is_disabled = false;
    checkbox.is_visible = true;
    
    // Initialize animation
    checkbox.check_animation = initial_checked ? 1.0f : 0.0f;
    checkbox.target_animation = checkbox.check_animation;
    
    // Initialize visual settings
    checkbox.show_label = true;
    checkbox.show_shadow = false;
    checkbox.show_border = true;
    
    // Initialize custom styling pointers to NULL
    checkbox.custom_box_color = NULL;
    checkbox.custom_check_color = NULL;
    checkbox.custom_border_color = NULL;
    checkbox.custom_label_color = NULL;
    checkbox.custom_padding = NULL;
    checkbox.custom_font_size = NULL;
    checkbox.custom_box_size = NULL;
    
    // Initialize callbacks
    checkbox.on_changed = NULL;
    checkbox.user_data = NULL;
    
    return checkbox;
}

void Checkbox_Destroy(Checkbox* checkbox) {
    if (!checkbox) return;
    
    if (checkbox->label) {
        free(checkbox->label);
        checkbox->label = NULL;
    }
    
    // Free custom styling
    if (checkbox->custom_box_color) { free(checkbox->custom_box_color); checkbox->custom_box_color = NULL; }
    if (checkbox->custom_check_color) { free(checkbox->custom_check_color); checkbox->custom_check_color = NULL; }
    if (checkbox->custom_border_color) { free(checkbox->custom_border_color); checkbox->custom_border_color = NULL; }
    if (checkbox->custom_label_color) { free(checkbox->custom_label_color); checkbox->custom_label_color = NULL; }
    if (checkbox->custom_padding) { free(checkbox->custom_padding); checkbox->custom_padding = NULL; }
    if (checkbox->custom_font_size) { free(checkbox->custom_font_size); checkbox->custom_font_size = NULL; }
    if (checkbox->custom_box_size) { free(checkbox->custom_box_size); checkbox->custom_box_size = NULL; }
}

void Checkbox_Update(Checkbox* checkbox) {
    if (!checkbox || !checkbox->is_visible || checkbox->is_disabled) {
        return;
    }
    
    Vector2 mouse_pos = GetMousePosition();
    bool mouse_over = CheckCollisionPointRec(mouse_pos, checkbox->bounds);
    
    checkbox->is_hovered = mouse_over;
    
    // Handle mouse interaction
    if (mouse_over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        checkbox->is_pressed = true;
    }
    
    if (checkbox->is_pressed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (mouse_over) {
            Checkbox_Toggle(checkbox);
        }
        checkbox->is_pressed = false;
    }
    
    // Update animation
    float animation_speed = AppTheme.checkbox_props.animation_speed * GetFrameTime();
    if (checkbox->check_animation < checkbox->target_animation) {
        checkbox->check_animation = fminf(checkbox->target_animation, 
                                         checkbox->check_animation + animation_speed);
    } else if (checkbox->check_animation > checkbox->target_animation) {
        checkbox->check_animation = fmaxf(checkbox->target_animation, 
                                         checkbox->check_animation - animation_speed);
    }
}

void Checkbox_Draw(Checkbox* checkbox) {
    if (!checkbox || !checkbox->is_visible) return;
    
    ComponentStyle* style = GetEffectiveStyle(checkbox);
    ComponentState state = GetCurrentState(checkbox);
    ColorPalette* colors = &style->colors[state];
    
    // Get effective values
    EdgeValues padding = checkbox->custom_padding ? *checkbox->custom_padding : style->padding;
    int font_size = checkbox->custom_font_size ? *checkbox->custom_font_size : style->font_size;
    float box_size = checkbox->custom_box_size ? *checkbox->custom_box_size : AppTheme.checkbox_props.box_size;
    
    Color box_color = checkbox->custom_box_color ? *checkbox->custom_box_color : colors->background;
    Color check_color = checkbox->custom_check_color ? *checkbox->custom_check_color : colors->accent;
    Color border_color = checkbox->custom_border_color ? *checkbox->custom_border_color : colors->border;
    Color label_color = checkbox->custom_label_color ? *checkbox->custom_label_color : colors->text;
    
    // Calculate checkbox box bounds
    Rectangle box_bounds = {
        checkbox->bounds.x + padding.left,
        checkbox->bounds.y + padding.top + (checkbox->bounds.height - box_size) * 0.5f,
        box_size,
        box_size
    };
    
    // Draw shadow
    if (checkbox->show_shadow) {
        ShadowConfig shadow_config = Shadow_FromTheme(style, state);
        Shadow_DrawRectangleRounded(box_bounds, style->border_radius.top_left, &shadow_config);
    }
    
    // Animate colors for checked state
    if (checkbox->is_checked && checkbox->check_animation > 0.0f) {
        float anim_progress = EaseOutCubic(checkbox->check_animation);
        
        // Interpolate to checked colors
        ColorPalette* active_colors = &style->colors[STATE_ACTIVE];
        box_color = (Color){
            (unsigned char)(box_color.r + (active_colors->background.r - box_color.r) * anim_progress),
            (unsigned char)(box_color.g + (active_colors->background.g - box_color.g) * anim_progress),
            (unsigned char)(box_color.b + (active_colors->background.b - box_color.b) * anim_progress),
            box_color.a
        };
        
        border_color = (Color){
            (unsigned char)(border_color.r + (active_colors->border.r - border_color.r) * anim_progress),
            (unsigned char)(border_color.g + (active_colors->border.g - border_color.g) * anim_progress),
            (unsigned char)(border_color.b + (active_colors->border.b - border_color.b) * anim_progress),
            border_color.a
        };
    }
    
    // Draw checkbox box
    DrawRectangleRounded(box_bounds, style->border_radius.top_left / box_bounds.height, 8, box_color);
    
    // Draw border
    if (checkbox->show_border) {
        DrawRectangleRoundedLines(box_bounds, style->border_radius.top_left / box_bounds.height, 
                                 8, border_color);
    }
    
    // Draw check mark
    if (AppTheme.checkbox_props.show_check_mark && checkbox->check_animation > 0.0f) {
        float anim_progress = EaseOutCubic(checkbox->check_animation);
        
        // Calculate check mark points
        Vector2 center = {box_bounds.x + box_bounds.width * 0.5f, box_bounds.y + box_bounds.height * 0.5f};
        float check_size = box_bounds.width * 0.6f * anim_progress;
        
        // Draw check mark as two lines forming a checkmark
        Vector2 p1 = {center.x - check_size * 0.3f, center.y};
        Vector2 p2 = {center.x - check_size * 0.1f, center.y + check_size * 0.2f};
        Vector2 p3 = {center.x + check_size * 0.4f, center.y - check_size * 0.3f};
        
        Color animated_check_color = check_color;
        animated_check_color.a = (unsigned char)(check_color.a * anim_progress);
        
        DrawLineEx(p1, p2, AppTheme.checkbox_props.check_mark_thickness, animated_check_color);
        DrawLineEx(p2, p3, AppTheme.checkbox_props.check_mark_thickness, animated_check_color);
    }
    
    // Draw label
    if (checkbox->show_label && checkbox->label) {
        Vector2 label_pos = {
            box_bounds.x + box_bounds.width + AppTheme.checkbox_props.label_spacing,
            checkbox->bounds.y + (checkbox->bounds.height - font_size) * 0.5f
        };
        
        DrawTextEx(GetFontDefault(), checkbox->label, label_pos, font_size, 1.0f, label_color);
    }
}

// State management functions
void Checkbox_SetChecked(Checkbox* checkbox, bool checked) {
    if (!checkbox || checkbox->is_disabled) return;
    
    bool was_checked = checkbox->is_checked;
    checkbox->is_checked = checked;
    checkbox->target_animation = checked ? 1.0f : 0.0f;
    
    if (was_checked != checked && checkbox->on_changed) {
        checkbox->on_changed(checked, checkbox->user_data);
    }
}

bool Checkbox_IsChecked(Checkbox* checkbox) {
    return checkbox ? checkbox->is_checked : false;
}

void Checkbox_Toggle(Checkbox* checkbox) {
    if (checkbox) {
        Checkbox_SetChecked(checkbox, !checkbox->is_checked);
    }
}

void Checkbox_SetDisabled(Checkbox* checkbox, bool disabled) {
    if (checkbox) {
        checkbox->is_disabled = disabled;
        if (disabled) {
            checkbox->is_hovered = false;
            checkbox->is_pressed = false;
        }
    }
}

void Checkbox_SetVisible(Checkbox* checkbox, bool visible) {
    if (checkbox) checkbox->is_visible = visible;
}

// Label management functions
void Checkbox_SetLabel(Checkbox* checkbox, const char* label) {
    if (!checkbox) return;
    
    if (checkbox->label) {
        free(checkbox->label);
        checkbox->label = NULL;
    }
    
    if (label) {
        size_t label_len = strlen(label) + 1;
        checkbox->label = (char*)malloc(label_len);
        strcpy(checkbox->label, label);
    }
}

const char* Checkbox_GetLabel(Checkbox* checkbox) {
    return checkbox ? checkbox->label : NULL;
}

// Visual customization functions
void Checkbox_SetShowLabel(Checkbox* checkbox, bool show) {
    if (checkbox) checkbox->show_label = show;
}

void Checkbox_SetShowShadow(Checkbox* checkbox, bool show) {
    if (checkbox) checkbox->show_shadow = show;
}

void Checkbox_SetShowBorder(Checkbox* checkbox, bool show) {
    if (checkbox) checkbox->show_border = show;
}

// Styling functions
void Checkbox_SetBoxColor(Checkbox* checkbox, Color color) {
    if (!checkbox) return;
    
    if (!checkbox->custom_box_color) {
        checkbox->custom_box_color = (Color*)malloc(sizeof(Color));
    }
    
    if (checkbox->custom_box_color) {
        *checkbox->custom_box_color = color;
    }
}

void Checkbox_SetCheckColor(Checkbox* checkbox, Color color) {
    if (!checkbox) return;
    
    if (!checkbox->custom_check_color) {
        checkbox->custom_check_color = (Color*)malloc(sizeof(Color));
    }
    
    if (checkbox->custom_check_color) {
        *checkbox->custom_check_color = color;
    }
}

void Checkbox_SetBorderColor(Checkbox* checkbox, Color color) {
    if (!checkbox) return;
    
    if (!checkbox->custom_border_color) {
        checkbox->custom_border_color = (Color*)malloc(sizeof(Color));
    }
    
    if (checkbox->custom_border_color) {
        *checkbox->custom_border_color = color;
    }
}

void Checkbox_SetLabelColor(Checkbox* checkbox, Color color) {
    if (!checkbox) return;
    
    if (!checkbox->custom_label_color) {
        checkbox->custom_label_color = (Color*)malloc(sizeof(Color));
    }
    
    if (checkbox->custom_label_color) {
        *checkbox->custom_label_color = color;
    }
}

void Checkbox_SetPadding(Checkbox* checkbox, float top, float right, float bottom, float left) {
    if (!checkbox) return;
    
    if (!checkbox->custom_padding) {
        checkbox->custom_padding = (EdgeValues*)malloc(sizeof(EdgeValues));
    }
    
    if (checkbox->custom_padding) {
        checkbox->custom_padding->top = top;
        checkbox->custom_padding->right = right;
        checkbox->custom_padding->bottom = bottom;
        checkbox->custom_padding->left = left;
    }
}

void Checkbox_SetFontSize(Checkbox* checkbox, int font_size) {
    if (!checkbox) return;
    
    if (!checkbox->custom_font_size) {
        checkbox->custom_font_size = (int*)malloc(sizeof(int));
    }
    
    if (checkbox->custom_font_size) {
        *checkbox->custom_font_size = font_size;
    }
}

void Checkbox_SetBoxSize(Checkbox* checkbox, float size) {
    if (!checkbox) return;
    
    if (!checkbox->custom_box_size) {
        checkbox->custom_box_size = (float*)malloc(sizeof(float));
    }
    
    if (checkbox->custom_box_size) {
        *checkbox->custom_box_size = size;
    }
}

// Callback functions
void Checkbox_SetOnChanged(Checkbox* checkbox, void (*callback)(bool, void*), void* user_data) {
    if (!checkbox) return;
    
    checkbox->on_changed = callback;
    checkbox->user_data = user_data;
}

// Utility functions
void Checkbox_ResetCustomStyles(Checkbox* checkbox) {
    if (!checkbox) return;
    
    if (checkbox->custom_box_color) { free(checkbox->custom_box_color); checkbox->custom_box_color = NULL; }
    if (checkbox->custom_check_color) { free(checkbox->custom_check_color); checkbox->custom_check_color = NULL; }
    if (checkbox->custom_border_color) { free(checkbox->custom_border_color); checkbox->custom_border_color = NULL; }
    if (checkbox->custom_label_color) { free(checkbox->custom_label_color); checkbox->custom_label_color = NULL; }
    if (checkbox->custom_padding) { free(checkbox->custom_padding); checkbox->custom_padding = NULL; }
    if (checkbox->custom_font_size) { free(checkbox->custom_font_size); checkbox->custom_font_size = NULL; }
    if (checkbox->custom_box_size) { free(checkbox->custom_box_size); checkbox->custom_box_size = NULL; }
}

Rectangle Checkbox_GetBoxBounds(Checkbox* checkbox) {
    if (!checkbox) return (Rectangle){0, 0, 0, 0};
    
    ComponentStyle* style = GetEffectiveStyle(checkbox);
    EdgeValues padding = checkbox->custom_padding ? *checkbox->custom_padding : style->padding;
    float box_size = checkbox->custom_box_size ? *checkbox->custom_box_size : AppTheme.checkbox_props.box_size;
    
    return (Rectangle){
        checkbox->bounds.x + padding.left,
        checkbox->bounds.y + padding.top + (checkbox->bounds.height - box_size) * 0.5f,
        box_size,
        box_size
    };
}

Rectangle Checkbox_GetLabelBounds(Checkbox* checkbox) {
    if (!checkbox || !checkbox->label) return (Rectangle){0, 0, 0, 0};
    
    ComponentStyle* style = GetEffectiveStyle(checkbox);
    int font_size = checkbox->custom_font_size ? *checkbox->custom_font_size : style->font_size;
    float box_size = checkbox->custom_box_size ? *checkbox->custom_box_size : AppTheme.checkbox_props.box_size;
    
    Rectangle box_bounds = Checkbox_GetBoxBounds(checkbox);
    Vector2 text_size = MeasureTextEx(GetFontDefault(), checkbox->label, font_size, 1.0f);
    
    return (Rectangle){
        box_bounds.x + box_bounds.width + AppTheme.checkbox_props.label_spacing,
        checkbox->bounds.y + (checkbox->bounds.height - text_size.y) * 0.5f,
        text_size.x,
        text_size.y
    };
}