#ifndef CARD_H
#define CARD_H

#include "raylib.h"
#include "component_base.h"
#include <stdbool.h>

// Card styles
typedef enum {
    CARD_STYLE_FLAT,
    CARD_STYLE_ELEVATED,
    CARD_STYLE_OUTLINED,
    CARD_STYLE_FILLED
} CardStyle;

// Card header positions
typedef enum {
    CARD_HEADER_TOP,
    CARD_HEADER_BOTTOM,
    CARD_HEADER_LEFT,
    CARD_HEADER_RIGHT,
    CARD_HEADER_NONE
} CardHeaderPosition;

typedef struct {
    // Base component
    ComponentBase base;
    
    // Card properties
    char title[256];
    char subtitle[256];
    CardStyle style;
    CardHeaderPosition header_position;
    
    // Visual properties
    Color background_color;
    Color border_color;
    Color title_color;
    Color subtitle_color;
    Color shadow_color;
    
    // Dimensions
    float padding;
    float border_width;
    float corner_radius;
    float shadow_offset_x;
    float shadow_offset_y;
    float shadow_blur;
    float elevation;
    
    // Header
    bool show_header;
    bool show_title;
    bool show_subtitle;
    float header_height;
    float header_padding;
    
    // Content
    ComponentBase* content;
    Rectangle content_rect;
    bool clip_content;
    
    // Interaction
    bool is_hovered;
    bool is_pressed;
    bool is_interactive;
    bool show_hover_effect;
    bool show_press_effect;
    
    // Animation
    float hover_scale;
    float press_scale;
    float current_scale;
    float animation_speed;
    bool is_animating;
    
    // Callbacks
    void (*on_click)(struct Card* card);
    void (*on_hover)(struct Card* card, bool is_hovered);
    void (*on_press)(struct Card* card, bool is_pressed);
} Card;

// Core Functions
Card Card_Create(Rectangle rect, CardStyle style);
bool Card_Init(Card* card, Rectangle rect, CardStyle style);
void Card_Update(Card* card);
void Card_Draw(const Card* card);
void Card_Destroy(Card* card);

// Card Properties
void Card_SetTitle(Card* card, const char* title);
void Card_SetSubtitle(Card* card, const char* subtitle);
void Card_SetStyle(Card* card, CardStyle style);
void Card_SetHeaderPosition(Card* card, CardHeaderPosition position);
const char* Card_GetTitle(const Card* card);
const char* Card_GetSubtitle(const Card* card);
CardStyle Card_GetStyle(const Card* card);
CardHeaderPosition Card_GetHeaderPosition(const Card* card);

// Visual Customization
void Card_SetColors(Card* card, Color background, Color border, Color title, Color subtitle);
void Card_SetBackgroundColor(Card* card, Color color);
void Card_SetBorderColor(Card* card, Color color);
void Card_SetTitleColor(Card* card, Color color);
void Card_SetSubtitleColor(Card* card, Color color);
void Card_SetShadowColor(Card* card, Color color);

// Dimensions
void Card_SetPadding(Card* card, float padding);
void Card_SetBorderWidth(Card* card, float width);
void Card_SetCornerRadius(Card* card, float radius);
void Card_SetShadowOffset(Card* card, float offset_x, float offset_y);
void Card_SetShadowBlur(Card* card, float blur);
void Card_SetElevation(Card* card, float elevation);

// Header
void Card_SetShowHeader(Card* card, bool show);
void Card_SetShowTitle(Card* card, bool show);
void Card_SetShowSubtitle(Card* card, bool show);
void Card_SetHeaderHeight(Card* card, float height);
void Card_SetHeaderPadding(Card* card, float padding);
bool Card_ShowHeader(const Card* card);
bool Card_ShowTitle(const Card* card);
bool Card_ShowSubtitle(const Card* card);
float Card_GetHeaderHeight(const Card* card);

// Content
bool Card_SetContent(Card* card, ComponentBase* content);
ComponentBase* Card_GetContent(const Card* card);
void Card_SetClipContent(Card* card, bool clip);
Rectangle Card_GetContentRect(const Card* card);
bool Card_ClipContent(const Card* card);

// Interaction
void Card_SetInteractive(Card* card, bool interactive);
void Card_SetShowHoverEffect(Card* card, bool show);
void Card_SetShowPressEffect(Card* card, bool show);
bool Card_IsInteractive(const Card* card);
bool Card_IsHovered(const Card* card);
bool Card_IsPressed(const Card* card);
bool Card_ShowHoverEffect(const Card* card);
bool Card_ShowPressEffect(const Card* card);

// Animation
void Card_SetHoverScale(Card* card, float scale);
void Card_SetPressScale(Card* card, float scale);
void Card_SetAnimationSpeed(Card* card, float speed);
float Card_GetHoverScale(const Card* card);
float Card_GetPressScale(const Card* card);
float Card_GetCurrentScale(const Card* card);
bool Card_IsAnimating(const Card* card);

// Callbacks
void Card_SetOnClickCallback(Card* card, void (*callback)(Card* card));
void Card_SetOnHoverCallback(Card* card, void (*callback)(Card* card, bool is_hovered));
void Card_SetOnPressCallback(Card* card, void (*callback)(Card* card, bool is_pressed));

// Input Handling
bool Card_HandleMouseClick(Card* card, Vector2 position);
bool Card_HandleMouseHover(Card* card, Vector2 position);

// Utility Functions
const char* Card_StyleToString(CardStyle style);
CardStyle Card_StringToStyle(const char* style_string);
const char* Card_HeaderPositionToString(CardHeaderPosition position);
CardHeaderPosition Card_StringToHeaderPosition(const char* position_string);

// Preset Styles
void Card_SetPresetFlat(Card* card);
void Card_SetPresetElevated(Card* card);
void Card_SetPresetOutlined(Card* card);
void Card_SetPresetFilled(Card* card);

#endif // CARD_H
