#ifndef SHADOW_H
#define SHADOW_H

#include "raylib.h"
#include "theme.h"

typedef enum {
    SHADOW_TYPE_SIMPLE,     // Basic colored rectangle shadow
    SHADOW_TYPE_SOFT,       // Blurred shadow using blur effect
    SHADOW_TYPE_INSET,      // Inner shadow effect
    SHADOW_TYPE_GLOW        // Glow effect (colored blur)
} ShadowType;

typedef struct {
    ShadowType type;
    Vector2 offset;
    float blur_radius;
    float spread;
    Color color;
    float opacity;
} ShadowConfig;

// Function declarations
ShadowConfig Shadow_FromTheme(ComponentStyle* style, ComponentState state);
ShadowConfig Shadow_Create(ShadowType type, Vector2 offset, float blur, float spread, Color color);

// Basic shadow rendering
void Shadow_DrawRectangle(Rectangle bounds, ShadowConfig* config);
void Shadow_DrawRectangleRounded(Rectangle bounds, float radius, ShadowConfig* config);
void Shadow_DrawCircle(Vector2 center, float radius, ShadowConfig* config);

// Advanced shadow rendering with custom shapes
void Shadow_DrawTexture(Texture2D texture, Rectangle source, Rectangle dest, ShadowConfig* config);
void Shadow_DrawCustomShape(void (*draw_shape)(Vector2 offset, Color color), ShadowConfig* config);

// Utility functions
void Shadow_DrawComponentShadow(Rectangle bounds, ComponentType type, ComponentState state);
bool Shadow_IsVisible(ShadowConfig* config);
Rectangle Shadow_GetBounds(Rectangle original_bounds, ShadowConfig* config);

#endif // SHADOW_H