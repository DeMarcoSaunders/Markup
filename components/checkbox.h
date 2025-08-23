#ifndef CHECKBOX_H
#define CHECKBOX_H

#include "raylib.h"
#include "theme.h"
#include <stdbool.h>

typedef struct {
    Rectangle bounds;
    char* label;
    bool is_checked;
    
    // State
    bool is_hovered;
    bool is_pressed;
    bool is_disabled;
    bool is_visible;
    
    // Animation
    float check_animation;
    float target_animation;
    
    // Visual customization
    bool show_label;
    bool show_shadow;
    bool show_border;
    
    // Custom styling (optional overrides)
    Color* custom_box_color;
    Color* custom_check_color;
    Color* custom_border_color;
    Color* custom_label_color;
    EdgeValues* custom_padding;
    int* custom_font_size;
    float* custom_box_size;
    
    // Callbacks
    void (*on_changed)(bool is_checked, void* user_data);
    void* user_data;
} Checkbox;

// Creation and destruction
Checkbox Checkbox_Create(Rectangle bounds, const char* label, bool initial_checked);
void Checkbox_Destroy(Checkbox* checkbox);

// Update and drawing
void Checkbox_Update(Checkbox* checkbox);
void Checkbox_Draw(Checkbox* checkbox);

// State management
void Checkbox_SetChecked(Checkbox* checkbox, bool checked);
bool Checkbox_IsChecked(Checkbox* checkbox);
void Checkbox_Toggle(Checkbox* checkbox);
void Checkbox_SetDisabled(Checkbox* checkbox, bool disabled);
void Checkbox_SetVisible(Checkbox* checkbox, bool visible);

// Label management
void Checkbox_SetLabel(Checkbox* checkbox, const char* label);
const char* Checkbox_GetLabel(Checkbox* checkbox);

// Visual customization
void Checkbox_SetShowLabel(Checkbox* checkbox, bool show);
void Checkbox_SetShowShadow(Checkbox* checkbox, bool show);
void Checkbox_SetShowBorder(Checkbox* checkbox, bool show);

// Styling
void Checkbox_SetBoxColor(Checkbox* checkbox, Color color);
void Checkbox_SetCheckColor(Checkbox* checkbox, Color color);
void Checkbox_SetBorderColor(Checkbox* checkbox, Color color);
void Checkbox_SetLabelColor(Checkbox* checkbox, Color color);
void Checkbox_SetPadding(Checkbox* checkbox, float top, float right, float bottom, float left);
void Checkbox_SetFontSize(Checkbox* checkbox, int font_size);
void Checkbox_SetBoxSize(Checkbox* checkbox, float size);

// Callbacks
void Checkbox_SetOnChanged(Checkbox* checkbox, void (*callback)(bool, void*), void* user_data);

// Utility functions
void Checkbox_ResetCustomStyles(Checkbox* checkbox);
Rectangle Checkbox_GetBoxBounds(Checkbox* checkbox);
Rectangle Checkbox_GetLabelBounds(Checkbox* checkbox);

#endif // CHECKBOX_H