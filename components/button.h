#ifndef BUTTON_H
#define BUTTON_H

#include "raylib.h"
#include <stdbool.h>

#include "theme.h"

typedef enum {
    BUTTON_PRIMARY,
    BUTTON_SECONDARY,
    BUTTON_DESTRUCTIVE
} ButtonVariant;

typedef struct {
    Rectangle rect;
    char* text;
    ButtonVariant variant;
    bool is_hovered;
    bool is_pressed;
    bool is_clicked;
    bool is_focused;
    bool is_disabled;
    bool is_visible;
    
    // Visual customization overrides (optional - NULL uses theme defaults)
    EdgeValues* custom_padding;
    EdgeValues* custom_margin;
    CornerRadius* custom_border_radius;
    Color* custom_background_color;
    Color* custom_text_color;
    Color* custom_border_color;
    int* custom_font_size;
    TextAlign* custom_text_align;
    TextVerticalAlign* custom_text_vertical_align;
    float* custom_opacity;
    Vector2* custom_shadow_offset;
    float* custom_shadow_blur;
    
    // Visual state flags
    bool show_border;
    bool show_outline;
    bool show_shadow;
    
    // Layout and positioning
    int z_index;
    Vector2 transform_offset;  // For animations/transforms
    float rotation;            // Rotation in degrees
    Vector2 scale;            // Scale factor (1.0 = normal)
} Button;

// Core Functions
Button Button_Create(Rectangle rect, const char* text, ButtonVariant variant);
void Button_Update(Button* button);
void Button_Draw(const Button* button);
void Button_Destroy(Button* button);
bool Button_IsClicked(const Button* button);

// Visual State Functions
void Button_SetBorder(Button* button, bool show_border);
void Button_SetOutline(Button* button, bool show_outline);
void Button_SetShadow(Button* button, bool show_shadow);
void Button_SetVisible(Button* button, bool is_visible);
void Button_SetDisabled(Button* button, bool is_disabled);

// CSS-like Styling Functions
void Button_SetPadding(Button* button, float top, float right, float bottom, float left);
void Button_SetMargin(Button* button, float top, float right, float bottom, float left);
void Button_SetBorderRadius(Button* button, float top_left, float top_right, float bottom_right, float bottom_left);
void Button_SetBackgroundColor(Button* button, Color color);
void Button_SetTextColor(Button* button, Color color);
void Button_SetBorderColor(Button* button, Color color);
void Button_SetFontSize(Button* button, int font_size);
void Button_SetTextAlign(Button* button, TextAlign horizontal, TextVerticalAlign vertical);
void Button_SetOpacity(Button* button, float opacity);
void Button_SetShadowStyle(Button* button, Vector2 offset, float blur, float spread, Color color);
void Button_SetTransform(Button* button, Vector2 offset, float rotation, Vector2 scale);
void Button_SetZIndex(Button* button, int z_index);

// CSS Reset Functions
void Button_ResetPadding(Button* button);
void Button_ResetMargin(Button* button);
void Button_ResetAllCustomStyles(Button* button);

#endif // BUTTON_H