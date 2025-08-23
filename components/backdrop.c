#include "backdrop.h"
#include "blur_effect.h"

BackdropConfig Backdrop_CreateOverlay(Color overlay_color) {
    return (BackdropConfig){
        .type = BACKDROP_OVERLAY,
        .overlay_color = overlay_color,
        .blur_strength = 0.0f,
        .animate_blur = false,
        .animation_progress = 1.0f
    };
}

BackdropConfig Backdrop_CreateBlur(float blur_strength) {
    return (BackdropConfig){
        .type = BACKDROP_BLUR,
        .overlay_color = BLANK,
        .blur_strength = blur_strength,
        .animate_blur = true,
        .animation_progress = 1.0f
    };
}

BackdropConfig Backdrop_CreateBlurOverlay(float blur_strength, Color overlay_color) {
    return (BackdropConfig){
        .type = BACKDROP_BLUR_OVERLAY,
        .overlay_color = overlay_color,
        .blur_strength = blur_strength,
        .animate_blur = true,
        .animation_progress = 1.0f
    };
}

void Backdrop_Draw(BackdropConfig* config, void (*draw_background)(void)) {
    Backdrop_DrawAnimated(config, draw_background, config->animation_progress);
}

void Backdrop_DrawAnimated(BackdropConfig* config, void (*draw_background)(void), float progress) {
    if (!config || progress <= 0.0f) {
        return;
    }
    
    switch (config->type) {
        case BACKDROP_NONE:
            if (draw_background) {
                draw_background();
            }
            break;
            
        case BACKDROP_OVERLAY: {
            if (draw_background) {
                draw_background();
            }
            Color overlay = config->overlay_color;
            overlay.a = (unsigned char)(overlay.a * progress);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), overlay);
            break;
        }
        
        case BACKDROP_BLUR: {
            if (BlurEffect_IsReady() && draw_background) {
                // Capture background
                BlurEffect_BeginCapture();
                draw_background();
                BlurEffect_EndCapture();
                
                // Apply blur with animation
                float animated_strength = config->animate_blur ? 
                    config->blur_strength * progress : 
                    config->blur_strength;
                BlurEffect_ApplyBlur(animated_strength);
                
                // Draw blurred result
                BlurEffect_DrawBlurred();
            } else if (draw_background) {
                // Fallback: draw background normally
                draw_background();
            }
            break;
        }
        
        case BACKDROP_BLUR_OVERLAY: {
            if (BlurEffect_IsReady() && draw_background) {
                // Capture background
                BlurEffect_BeginCapture();
                draw_background();
                BlurEffect_EndCapture();
                
                // Apply blur with animation
                float animated_strength = config->animate_blur ? 
                    config->blur_strength * progress : 
                    config->blur_strength;
                BlurEffect_ApplyBlur(animated_strength);
                
                // Draw blurred result
                BlurEffect_DrawBlurred();
                
                // Add overlay on top
                Color overlay = config->overlay_color;
                overlay.a = (unsigned char)(overlay.a * progress);
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), overlay);
            } else if (draw_background) {
                // Fallback: draw background with overlay
                draw_background();
                Color overlay = config->overlay_color;
                overlay.a = (unsigned char)(overlay.a * progress);
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), overlay);
            }
            break;
        }
    }
}

void Backdrop_SetAnimationProgress(BackdropConfig* config, float progress) {
    if (config) {
        config->animation_progress = progress;
    }
}

void Backdrop_DrawModalBackdrop(void (*draw_background)(void), float blur_strength, Color overlay, float progress) {
    BackdropConfig config = Backdrop_CreateBlurOverlay(blur_strength, overlay);
    Backdrop_DrawAnimated(&config, draw_background, progress);
}

void Backdrop_DrawShadowBackdrop(Rectangle bounds, float blur_radius, Color shadow_color) {
    if (!BlurEffect_IsReady() || blur_radius <= 0.0f) {
        // Fallback: draw simple shadow rectangle
        DrawRectangle((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height, shadow_color);
        return;
    }
    
    // Create a temporary texture for the shadow shape
    RenderTexture2D shadow_rt = LoadRenderTexture((int)bounds.width + (int)(blur_radius * 4), 
                                                  (int)bounds.height + (int)(blur_radius * 4));
    
    if (shadow_rt.id == 0) {
        // Fallback
        DrawRectangle((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height, shadow_color);
        return;
    }
    
    // Draw the shadow shape to the texture
    BeginTextureMode(shadow_rt);
    ClearBackground(BLANK);
    DrawRectangle((int)(blur_radius * 2), (int)(blur_radius * 2), 
                  (int)bounds.width, (int)bounds.height, shadow_color);
    EndTextureMode();
    
    // Apply blur to create soft shadow
    RenderTexture2D blur_rt = LoadRenderTexture(shadow_rt.texture.width, shadow_rt.texture.height);
    if (blur_rt.id != 0) {
        BlurEffect_BlurTexture(&shadow_rt, &blur_rt, blur_radius);
        
        // Draw the blurred shadow
        Rectangle dest = {
            bounds.x - blur_radius * 2,
            bounds.y - blur_radius * 2,
            bounds.width + blur_radius * 4,
            bounds.height + blur_radius * 4
        };
        
        DrawTextureRec(shadow_rt.texture, 
                       (Rectangle){0, 0, (float)shadow_rt.texture.width, -(float)shadow_rt.texture.height},
                       (Vector2){dest.x, dest.y}, WHITE);
        
        UnloadRenderTexture(blur_rt);
    }
    
    UnloadRenderTexture(shadow_rt);
}