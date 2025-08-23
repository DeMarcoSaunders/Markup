#ifndef BLUR_EFFECT_H
#define BLUR_EFFECT_H

#include "raylib.h"

typedef struct {
    Shader blur_shader;
    RenderTexture2D temp_texture;
    RenderTexture2D blur_texture;
    bool is_initialized;
    int resolution_loc;
    int direction_loc;
    int blur_strength_loc;
} BlurEffect;

// Global blur effect instance
extern BlurEffect AppBlurEffect;

// Function declarations
bool BlurEffect_Init(void);
void BlurEffect_Destroy(void);
void BlurEffect_Resize(int width, int height);

// Generic blur functions - can be used by any component
void BlurEffect_BeginCapture(void);
void BlurEffect_EndCapture(void);
void BlurEffect_ApplyBlur(float strength);
void BlurEffect_DrawBlurred(void);
void BlurEffect_DrawBlurredTinted(Color tint);

// Advanced blur functions for custom use cases
RenderTexture2D* BlurEffect_GetBlurredTexture(void);
void BlurEffect_BlurTexture(RenderTexture2D* source, RenderTexture2D* destination, float strength);
void BlurEffect_DrawTextureBlurred(Texture2D texture, Rectangle source, Rectangle dest, float strength, Color tint);

// Utility functions
bool BlurEffect_IsReady(void);
Vector2 BlurEffect_GetTextureSize(void);

#endif // BLUR_EFFECT_H