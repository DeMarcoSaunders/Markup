#ifndef BACKDROP_H
#define BACKDROP_H

#include "raylib.h"
#include <stdbool.h>

typedef enum {
    BACKDROP_NONE,
    BACKDROP_OVERLAY,
    BACKDROP_BLUR,
    BACKDROP_BLUR_OVERLAY
} BackdropType;

typedef struct {
    BackdropType type;
    Color overlay_color;
    float blur_strength;
    bool animate_blur;
    float animation_progress;
} BackdropConfig;

// Function declarations
BackdropConfig Backdrop_CreateOverlay(Color overlay_color);
BackdropConfig Backdrop_CreateBlur(float blur_strength);
BackdropConfig Backdrop_CreateBlurOverlay(float blur_strength, Color overlay_color);

void Backdrop_Draw(BackdropConfig* config, void (*draw_background)(void));
void Backdrop_DrawAnimated(BackdropConfig* config, void (*draw_background)(void), float progress);
void Backdrop_SetAnimationProgress(BackdropConfig* config, float progress);

// Utility functions for common backdrop effects
void Backdrop_DrawModalBackdrop(void (*draw_background)(void), float blur_strength, Color overlay, float progress);
void Backdrop_DrawShadowBackdrop(Rectangle bounds, float blur_radius, Color shadow_color);

#endif // BACKDROP_H