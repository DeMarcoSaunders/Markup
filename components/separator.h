#ifndef SEPARATOR_H
#define SEPARATOR_H

#include "raylib.h"
#include <stdbool.h>

// Separator types
typedef enum {
    SEPARATOR_HORIZONTAL,
    SEPARATOR_VERTICAL,
    SEPARATOR_SPACER
} SeparatorType;

// Line styles
typedef enum {
    SEPARATOR_SOLID,
    SEPARATOR_DASHED,
    SEPARATOR_DOTTED
} SeparatorStyle;

// Text alignment for labeled separators
typedef enum {
    SEPARATOR_TEXT_LEFT,
    SEPARATOR_TEXT_CENTER,
    SEPARATOR_TEXT_RIGHT
} SeparatorTextAlign;

// Separator component structure
typedef struct {
    Rectangle bounds;
    SeparatorType type;
    SeparatorStyle style;
    
    // Visual properties
    Color color;
    float thickness;
    float opacity;
    bool visible;
    
    // Margins
    float margin_top;
    float margin_right;
    float margin_bottom;
    float margin_left;
    
    // Text label (optional)
    char* text;
    SeparatorTextAlign text_align;
    Color text_color;
    int font_size;
    float text_padding;
    
    // Dashed/dotted properties
    float dash_length;
    float gap_length;
    
    // Animation
    float fade_in_duration;
    float current_fade_time;
    bool is_animating;
    
    // Z-index for layering
    int z_index;
} Separator;

// Creation and destruction
Separator Separator_Create(Rectangle bounds, SeparatorType type);
Separator Separator_CreateHorizontal(float x, float y, float width);
Separator Separator_CreateVertical(float x, float y, float height);
Separator Separator_CreateSpacer(float x, float y, float width, float height);
Separator Separator_CreateWithText(Rectangle bounds, const char* text, SeparatorTextAlign align);
void Separator_Destroy(Separator* separator);

// Basic properties
void Separator_SetType(Separator* separator, SeparatorType type);
void Separator_SetStyle(Separator* separator, SeparatorStyle style);
void Separator_SetColor(Separator* separator, Color color);
void Separator_SetThickness(Separator* separator, float thickness);
void Separator_SetOpacity(Separator* separator, float opacity);
void Separator_SetVisible(Separator* separator, bool visible);

// Margins
void Separator_SetMargin(Separator* separator, float top, float right, float bottom, float left);
void Separator_SetMarginHorizontal(Separator* separator, float horizontal);
void Separator_SetMarginVertical(Separator* separator, float vertical);
void Separator_SetMarginAll(Separator* separator, float margin);

// Text label
void Separator_SetText(Separator* separator, const char* text);
void Separator_SetTextAlign(Separator* separator, SeparatorTextAlign align);
void Separator_SetTextColor(Separator* separator, Color color);
void Separator_SetFontSize(Separator* separator, int font_size);
void Separator_SetTextPadding(Separator* separator, float padding);
void Separator_ClearText(Separator* separator);

// Dashed/dotted styling
void Separator_SetDashPattern(Separator* separator, float dash_length, float gap_length);

// Animation
void Separator_SetFadeIn(Separator* separator, float duration);
void Separator_StartFadeIn(Separator* separator);

// Layout
void Separator_SetBounds(Separator* separator, Rectangle bounds);
void Separator_SetPosition(Separator* separator, float x, float y);
void Separator_SetSize(Separator* separator, float width, float height);
void Separator_SetZIndex(Separator* separator, int z_index);

// Utility functions
Rectangle Separator_GetBounds(const Separator* separator);
Rectangle Separator_GetContentBounds(const Separator* separator); // Bounds minus margins
bool Separator_HasText(const Separator* separator);
float Separator_GetTextWidth(const Separator* separator);

// Update and drawing
void Separator_Update(Separator* separator);
void Separator_Draw(const Separator* separator);

// Preset creators for common use cases
Separator Separator_CreateMenuDivider(float x, float y, float width);
Separator Separator_CreateFormSection(float x, float y, float width, const char* section_title);
Separator Separator_CreateToolbarDivider(float x, float y, float height);
Separator Separator_CreateContentBreak(float x, float y, float width);

#endif // SEPARATOR_H