#include "blur_effect.h"
#include <stdio.h>

// Global blur effect instance
BlurEffect AppBlurEffect = {0};

bool BlurEffect_Init(void) {
    if (AppBlurEffect.is_initialized) {
        return true;
    }
    
    // Load the improved blur shader
    AppBlurEffect.blur_shader = LoadShader(0, "external/raylib/blur_improved.fs");
    
    if (AppBlurEffect.blur_shader.id == 0) {
        printf("Failed to load blur shader\n");
        return false;
    }
    
    // Get uniform locations
    AppBlurEffect.resolution_loc = GetShaderLocation(AppBlurEffect.blur_shader, "resolution");
    AppBlurEffect.direction_loc = GetShaderLocation(AppBlurEffect.blur_shader, "direction");
    AppBlurEffect.blur_strength_loc = GetShaderLocation(AppBlurEffect.blur_shader, "blurStrength");
    
    // Create render textures for blur passes
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();
    
    AppBlurEffect.temp_texture = LoadRenderTexture(screen_width, screen_height);
    AppBlurEffect.blur_texture = LoadRenderTexture(screen_width, screen_height);
    
    if (AppBlurEffect.temp_texture.id == 0 || AppBlurEffect.blur_texture.id == 0) {
        printf("Failed to create blur render textures\n");
        BlurEffect_Destroy();
        return false;
    }
    
    AppBlurEffect.is_initialized = true;
    return true;
}

void BlurEffect_Resize(int width, int height) {
    if (!AppBlurEffect.is_initialized) {
        return;
    }
    
    // Unload old textures
    if (AppBlurEffect.temp_texture.id != 0) {
        UnloadRenderTexture(AppBlurEffect.temp_texture);
    }
    if (AppBlurEffect.blur_texture.id != 0) {
        UnloadRenderTexture(AppBlurEffect.blur_texture);
    }
    
    // Create new textures with new size
    AppBlurEffect.temp_texture = LoadRenderTexture(width, height);
    AppBlurEffect.blur_texture = LoadRenderTexture(width, height);
    
    if (AppBlurEffect.temp_texture.id == 0 || AppBlurEffect.blur_texture.id == 0) {
        printf("Failed to resize blur render textures\n");
        AppBlurEffect.is_initialized = false;
    }
}

void BlurEffect_Destroy(void) {
    if (!AppBlurEffect.is_initialized) {
        return;
    }
    
    if (AppBlurEffect.blur_shader.id != 0) {
        UnloadShader(AppBlurEffect.blur_shader);
    }
    
    if (AppBlurEffect.temp_texture.id != 0) {
        UnloadRenderTexture(AppBlurEffect.temp_texture);
    }
    
    if (AppBlurEffect.blur_texture.id != 0) {
        UnloadRenderTexture(AppBlurEffect.blur_texture);
    }
    
    AppBlurEffect = (BlurEffect){0};
}

void BlurEffect_BeginCapture(void) {
    if (!AppBlurEffect.is_initialized) {
        return;
    }
    
    BeginTextureMode(AppBlurEffect.temp_texture);
    ClearBackground(BLANK);
}

void BlurEffect_EndCapture(void) {
    if (!AppBlurEffect.is_initialized) {
        return;
    }
    
    EndTextureMode();
}

void BlurEffect_ApplyBlur(float strength) {
    if (!AppBlurEffect.is_initialized) {
        return;
    }
    
    BlurEffect_BlurTexture(&AppBlurEffect.temp_texture, &AppBlurEffect.blur_texture, strength);
}

void BlurEffect_BlurTexture(RenderTexture2D* source, RenderTexture2D* destination, float strength) {
    if (!AppBlurEffect.is_initialized || !source || !destination) {
        return;
    }
    
    int texture_width = source->texture.width;
    int texture_height = source->texture.height;
    
    // Set shader uniforms
    float resolution[2] = {(float)texture_width, (float)texture_height};
    SetShaderValue(AppBlurEffect.blur_shader, AppBlurEffect.resolution_loc, resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(AppBlurEffect.blur_shader, AppBlurEffect.blur_strength_loc, &strength, SHADER_UNIFORM_FLOAT);
    
    // First pass: Horizontal blur
    BeginTextureMode(*destination);
    ClearBackground(BLANK);
    
    float horizontal_direction[2] = {1.0f, 0.0f};
    SetShaderValue(AppBlurEffect.blur_shader, AppBlurEffect.direction_loc, horizontal_direction, SHADER_UNIFORM_VEC2);
    
    BeginShaderMode(AppBlurEffect.blur_shader);
    DrawTextureRec(source->texture, 
                   (Rectangle){0, 0, (float)texture_width, -(float)texture_height}, 
                   (Vector2){0, 0}, WHITE);
    EndShaderMode();
    
    EndTextureMode();
    
    // Second pass: Vertical blur (back to source texture)
    BeginTextureMode(*source);
    ClearBackground(BLANK);
    
    float vertical_direction[2] = {0.0f, 1.0f};
    SetShaderValue(AppBlurEffect.blur_shader, AppBlurEffect.direction_loc, vertical_direction, SHADER_UNIFORM_VEC2);
    
    BeginShaderMode(AppBlurEffect.blur_shader);
    DrawTextureRec(destination->texture, 
                   (Rectangle){0, 0, (float)texture_width, -(float)texture_height}, 
                   (Vector2){0, 0}, WHITE);
    EndShaderMode();
    
    EndTextureMode();
}

void BlurEffect_DrawBlurred(void) {
    BlurEffect_DrawBlurredTinted(WHITE);
}

void BlurEffect_DrawBlurredTinted(Color tint) {
    if (!AppBlurEffect.is_initialized) {
        return;
    }
    
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();
    
    DrawTextureRec(AppBlurEffect.temp_texture.texture, 
                   (Rectangle){0, 0, (float)screen_width, -(float)screen_height}, 
                   (Vector2){0, 0}, tint);
}

void BlurEffect_DrawTextureBlurred(Texture2D texture, Rectangle source, Rectangle dest, float strength, Color tint) {
    if (!AppBlurEffect.is_initialized) {
        // Fallback: draw texture normally
        DrawTexturePro(texture, source, dest, (Vector2){0, 0}, 0.0f, tint);
        return;
    }
    
    // Create temporary render texture for this specific blur operation
    RenderTexture2D temp_rt = LoadRenderTexture((int)dest.width, (int)dest.height);
    RenderTexture2D blur_rt = LoadRenderTexture((int)dest.width, (int)dest.height);
    
    if (temp_rt.id == 0 || blur_rt.id == 0) {
        // Fallback: draw texture normally
        DrawTexturePro(texture, source, dest, (Vector2){0, 0}, 0.0f, tint);
        if (temp_rt.id != 0) UnloadRenderTexture(temp_rt);
        if (blur_rt.id != 0) UnloadRenderTexture(blur_rt);
        return;
    }
    
    // Render texture to temp render texture
    BeginTextureMode(temp_rt);
    ClearBackground(BLANK);
    DrawTexturePro(texture, source, (Rectangle){0, 0, dest.width, dest.height}, (Vector2){0, 0}, 0.0f, WHITE);
    EndTextureMode();
    
    // Apply blur
    BlurEffect_BlurTexture(&temp_rt, &blur_rt, strength);
    
    // Draw the blurred result
    DrawTextureRec(temp_rt.texture, 
                   (Rectangle){0, 0, dest.width, -dest.height}, 
                   (Vector2){dest.x, dest.y}, tint);
    
    // Cleanup
    UnloadRenderTexture(temp_rt);
    UnloadRenderTexture(blur_rt);
}

RenderTexture2D* BlurEffect_GetBlurredTexture(void) {
    if (!AppBlurEffect.is_initialized) {
        return NULL;
    }
    return &AppBlurEffect.temp_texture;
}

bool BlurEffect_IsReady(void) {
    return AppBlurEffect.is_initialized;
}

Vector2 BlurEffect_GetTextureSize(void) {
    if (!AppBlurEffect.is_initialized) {
        return (Vector2){0, 0};
    }
    return (Vector2){
        (float)AppBlurEffect.temp_texture.texture.width,
        (float)AppBlurEffect.temp_texture.texture.height
    };
}