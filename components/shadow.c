#include "shadow.h"
#include "blur_effect.h"
#include <math.h>

ShadowConfig Shadow_FromTheme(ComponentStyle* style, ComponentState state) {
    if (!style) {
        return (ShadowConfig){0};
    }
    
    ColorPalette* colors = &style->colors[state];
    
    return (ShadowConfig){
        .type = (style->shadow_blur > 0.0f) ? SHADOW_TYPE_SOFT : SHADOW_TYPE_SIMPLE,
        .offset = style->shadow_offset,
        .blur_radius = style->shadow_blur,
        .spread = style->shadow_spread,
        .color = colors->shadow,
        .opacity = style->opacity
    };
}

ShadowConfig Shadow_Create(ShadowType type, Vector2 offset, float blur, float spread, Color color) {
    return (ShadowConfig){
        .type = type,
        .offset = offset,
        .blur_radius = blur,
        .spread = spread,
        .color = color,
        .opacity = 1.0f
    };
}

static Color ApplyShadowOpacity(Color color, float opacity) {
    Color result = color;
    result.a = (unsigned char)(result.a * opacity);
    return result;
}

static Rectangle ExpandRectangle(Rectangle rect, float spread) {
    return (Rectangle){
        rect.x - spread,
        rect.y - spread,
        rect.width + spread * 2,
        rect.height + spread * 2
    };
}

void Shadow_DrawRectangle(Rectangle bounds, ShadowConfig* config) {
    if (!config || !Shadow_IsVisible(config)) {
        return;
    }
    
    Color shadow_color = ApplyShadowOpacity(config->color, config->opacity);
    Rectangle shadow_bounds = {
        bounds.x + config->offset.x,
        bounds.y + config->offset.y,
        bounds.width,
        bounds.height
    };
    
    // Apply spread
    if (config->spread != 0.0f) {
        shadow_bounds = ExpandRectangle(shadow_bounds, config->spread);
    }
    
    switch (config->type) {
        case SHADOW_TYPE_SIMPLE:
            DrawRectangle((int)shadow_bounds.x, (int)shadow_bounds.y, 
                         (int)shadow_bounds.width, (int)shadow_bounds.height, shadow_color);
            break;
            
        case SHADOW_TYPE_SOFT:
            if (BlurEffect_IsReady() && config->blur_radius > 0.0f) {
                // Create a temporary render texture for the shadow shape
                int shadow_size = (int)(fmaxf(shadow_bounds.width, shadow_bounds.height) + config->blur_radius * 4);
                RenderTexture2D shadow_rt = LoadRenderTexture(shadow_size, shadow_size);
                
                if (shadow_rt.id != 0) {
                    // Draw the shadow shape to the texture
                    BeginTextureMode(shadow_rt);
                    ClearBackground(BLANK);
                    
                    float center_x = shadow_size * 0.5f;
                    float center_y = shadow_size * 0.5f;
                    DrawRectangle((int)(center_x - shadow_bounds.width * 0.5f), 
                                 (int)(center_y - shadow_bounds.height * 0.5f),
                                 (int)shadow_bounds.width, (int)shadow_bounds.height, shadow_color);
                    EndTextureMode();
                    
                    // Apply blur
                    RenderTexture2D blur_rt = LoadRenderTexture(shadow_size, shadow_size);
                    if (blur_rt.id != 0) {
                        BlurEffect_BlurTexture(&shadow_rt, &blur_rt, config->blur_radius);
                        
                        // Draw the blurred shadow
                        Rectangle dest = {
                            shadow_bounds.x - config->blur_radius * 2,
                            shadow_bounds.y - config->blur_radius * 2,
                            (float)shadow_size,
                            (float)shadow_size
                        };
                        
                        DrawTextureRec(shadow_rt.texture, 
                                      (Rectangle){0, 0, (float)shadow_size, -(float)shadow_size},
                                      (Vector2){dest.x, dest.y}, WHITE);
                        
                        UnloadRenderTexture(blur_rt);
                    }
                    
                    UnloadRenderTexture(shadow_rt);
                } else {
                    // Fallback to simple shadow
                    DrawRectangle((int)shadow_bounds.x, (int)shadow_bounds.y, 
                                 (int)shadow_bounds.width, (int)shadow_bounds.height, shadow_color);
                }
            } else {
                // Fallback to simple shadow
                DrawRectangle((int)shadow_bounds.x, (int)shadow_bounds.y, 
                             (int)shadow_bounds.width, (int)shadow_bounds.height, shadow_color);
            }
            break;
            
        case SHADOW_TYPE_INSET:
            // Draw inset shadow (inside the bounds)
            DrawRectangleLines((int)bounds.x, (int)bounds.y, 
                              (int)bounds.width, (int)bounds.height, shadow_color);
            break;
            
        case SHADOW_TYPE_GLOW:
            if (BlurEffect_IsReady() && config->blur_radius > 0.0f) {
                // Similar to soft shadow but with glow color
                Shadow_DrawRectangle(bounds, &(ShadowConfig){
                    .type = SHADOW_TYPE_SOFT,
                    .offset = (Vector2){0, 0}, // No offset for glow
                    .blur_radius = config->blur_radius,
                    .spread = config->spread,
                    .color = config->color,
                    .opacity = config->opacity
                });
            }
            break;
    }
}

void Shadow_DrawRectangleRounded(Rectangle bounds, float radius, ShadowConfig* config) {
    if (!config || !Shadow_IsVisible(config)) {
        return;
    }
    
    Color shadow_color = ApplyShadowOpacity(config->color, config->opacity);
    Rectangle shadow_bounds = {
        bounds.x + config->offset.x,
        bounds.y + config->offset.y,
        bounds.width,
        bounds.height
    };
    
    // Apply spread
    if (config->spread != 0.0f) {
        shadow_bounds = ExpandRectangle(shadow_bounds, config->spread);
        radius += config->spread; // Adjust radius for spread
    }
    
    switch (config->type) {
        case SHADOW_TYPE_SIMPLE:
            DrawRectangleRounded(shadow_bounds, radius / shadow_bounds.height, 8, shadow_color);
            break;
            
        case SHADOW_TYPE_SOFT:
            if (BlurEffect_IsReady() && config->blur_radius > 0.0f) {
                // Create a temporary render texture for the rounded shadow shape
                int shadow_size = (int)(fmaxf(shadow_bounds.width, shadow_bounds.height) + config->blur_radius * 4);
                RenderTexture2D shadow_rt = LoadRenderTexture(shadow_size, shadow_size);
                
                if (shadow_rt.id != 0) {
                    // Draw the rounded shadow shape to the texture
                    BeginTextureMode(shadow_rt);
                    ClearBackground(BLANK);
                    
                    float center_x = shadow_size * 0.5f;
                    float center_y = shadow_size * 0.5f;
                    Rectangle shape_rect = {
                        center_x - shadow_bounds.width * 0.5f,
                        center_y - shadow_bounds.height * 0.5f,
                        shadow_bounds.width,
                        shadow_bounds.height
                    };
                    
                    DrawRectangleRounded(shape_rect, radius / shape_rect.height, 8, shadow_color);
                    EndTextureMode();
                    
                    // Apply blur
                    RenderTexture2D blur_rt = LoadRenderTexture(shadow_size, shadow_size);
                    if (blur_rt.id != 0) {
                        BlurEffect_BlurTexture(&shadow_rt, &blur_rt, config->blur_radius);
                        
                        // Draw the blurred shadow
                        Rectangle dest = {
                            shadow_bounds.x - config->blur_radius * 2,
                            shadow_bounds.y - config->blur_radius * 2,
                            (float)shadow_size,
                            (float)shadow_size
                        };
                        
                        DrawTextureRec(shadow_rt.texture, 
                                      (Rectangle){0, 0, (float)shadow_size, -(float)shadow_size},
                                      (Vector2){dest.x, dest.y}, WHITE);
                        
                        UnloadRenderTexture(blur_rt);
                    }
                    
                    UnloadRenderTexture(shadow_rt);
                } else {
                    // Fallback to simple rounded shadow
                    DrawRectangleRounded(shadow_bounds, radius / shadow_bounds.height, 8, shadow_color);
                }
            } else {
                // Fallback to simple rounded shadow
                DrawRectangleRounded(shadow_bounds, radius / shadow_bounds.height, 8, shadow_color);
            }
            break;
            
        case SHADOW_TYPE_INSET:
            DrawRectangleRoundedLines(bounds, radius / bounds.height, 8, shadow_color);
            break;
            
        case SHADOW_TYPE_GLOW:
            if (BlurEffect_IsReady() && config->blur_radius > 0.0f) {
                Shadow_DrawRectangleRounded(bounds, radius, &(ShadowConfig){
                    .type = SHADOW_TYPE_SOFT,
                    .offset = (Vector2){0, 0},
                    .blur_radius = config->blur_radius,
                    .spread = config->spread,
                    .color = config->color,
                    .opacity = config->opacity
                });
            }
            break;
    }
}

void Shadow_DrawCircle(Vector2 center, float radius, ShadowConfig* config) {
    if (!config || !Shadow_IsVisible(config)) {
        return;
    }
    
    Color shadow_color = ApplyShadowOpacity(config->color, config->opacity);
    Vector2 shadow_center = {
        center.x + config->offset.x,
        center.y + config->offset.y
    };
    
    float shadow_radius = radius + config->spread;
    
    switch (config->type) {
        case SHADOW_TYPE_SIMPLE:
            DrawCircle((int)shadow_center.x, (int)shadow_center.y, shadow_radius, shadow_color);
            break;
            
        case SHADOW_TYPE_SOFT:
            if (BlurEffect_IsReady() && config->blur_radius > 0.0f) {
                // Create a temporary render texture for the circular shadow
                int shadow_size = (int)((shadow_radius + config->blur_radius) * 4);
                RenderTexture2D shadow_rt = LoadRenderTexture(shadow_size, shadow_size);
                
                if (shadow_rt.id != 0) {
                    // Draw the circular shadow shape to the texture
                    BeginTextureMode(shadow_rt);
                    ClearBackground(BLANK);
                    
                    float center_pos = shadow_size * 0.5f;
                    DrawCircle((int)center_pos, (int)center_pos, shadow_radius, shadow_color);
                    EndTextureMode();
                    
                    // Apply blur
                    RenderTexture2D blur_rt = LoadRenderTexture(shadow_size, shadow_size);
                    if (blur_rt.id != 0) {
                        BlurEffect_BlurTexture(&shadow_rt, &blur_rt, config->blur_radius);
                        
                        // Draw the blurred shadow
                        Vector2 dest_pos = {
                            shadow_center.x - shadow_size * 0.5f,
                            shadow_center.y - shadow_size * 0.5f
                        };
                        
                        DrawTextureRec(shadow_rt.texture, 
                                      (Rectangle){0, 0, (float)shadow_size, -(float)shadow_size},
                                      dest_pos, WHITE);
                        
                        UnloadRenderTexture(blur_rt);
                    }
                    
                    UnloadRenderTexture(shadow_rt);
                } else {
                    // Fallback to simple circular shadow
                    DrawCircle((int)shadow_center.x, (int)shadow_center.y, shadow_radius, shadow_color);
                }
            } else {
                // Fallback to simple circular shadow
                DrawCircle((int)shadow_center.x, (int)shadow_center.y, shadow_radius, shadow_color);
            }
            break;
            
        case SHADOW_TYPE_INSET:
            DrawCircleLines((int)center.x, (int)center.y, radius, shadow_color);
            break;
            
        case SHADOW_TYPE_GLOW:
            if (BlurEffect_IsReady() && config->blur_radius > 0.0f) {
                Shadow_DrawCircle(center, radius, &(ShadowConfig){
                    .type = SHADOW_TYPE_SOFT,
                    .offset = (Vector2){0, 0},
                    .blur_radius = config->blur_radius,
                    .spread = config->spread,
                    .color = config->color,
                    .opacity = config->opacity
                });
            }
            break;
    }
}

void Shadow_DrawTexture(Texture2D texture, Rectangle source, Rectangle dest, ShadowConfig* config) {
    if (!config || !Shadow_IsVisible(config)) {
        return;
    }
    
    // For texture shadows, we'll create a silhouette and blur it
    if (config->type == SHADOW_TYPE_SOFT && BlurEffect_IsReady() && config->blur_radius > 0.0f) {
        Color shadow_color = ApplyShadowOpacity(config->color, config->opacity);
        Rectangle shadow_dest = {
            dest.x + config->offset.x,
            dest.y + config->offset.y,
            dest.width,
            dest.height
        };
        
        if (config->spread != 0.0f) {
            shadow_dest = ExpandRectangle(shadow_dest, config->spread);
        }
        
        // Create shadow silhouette by drawing texture in shadow color
        BlurEffect_DrawTextureBlurred(texture, source, shadow_dest, config->blur_radius, shadow_color);
    } else {
        // Simple shadow: draw texture tinted with shadow color
        Color shadow_color = ApplyShadowOpacity(config->color, config->opacity);
        Rectangle shadow_dest = {
            dest.x + config->offset.x,
            dest.y + config->offset.y,
            dest.width,
            dest.height
        };
        
        DrawTexturePro(texture, source, shadow_dest, (Vector2){0, 0}, 0.0f, shadow_color);
    }
}

void Shadow_DrawCustomShape(void (*draw_shape)(Vector2 offset, Color color), ShadowConfig* config) {
    if (!config || !Shadow_IsVisible(config) || !draw_shape) {
        return;
    }
    
    Color shadow_color = ApplyShadowOpacity(config->color, config->opacity);
    
    if (config->type == SHADOW_TYPE_SOFT && BlurEffect_IsReady() && config->blur_radius > 0.0f) {
        // For custom shapes with blur, we'd need to render to texture first
        // This is a simplified implementation
        draw_shape(config->offset, shadow_color);
    } else {
        // Simple shadow
        draw_shape(config->offset, shadow_color);
    }
}

void Shadow_DrawComponentShadow(Rectangle bounds, ComponentType type, ComponentState state) {
    ComponentStyle* style = Theme_GetComponentStyle(type);
    if (!style) return;
    
    ShadowConfig config = Shadow_FromTheme(style, state);
    
    // Use rounded rectangle for most components
    float radius = style->border_radius.top_left;
    if (radius > 0.0f) {
        Shadow_DrawRectangleRounded(bounds, radius, &config);
    } else {
        Shadow_DrawRectangle(bounds, &config);
    }
}

bool Shadow_IsVisible(ShadowConfig* config) {
    return config && config->color.a > 0 && config->opacity > 0.0f;
}

Rectangle Shadow_GetBounds(Rectangle original_bounds, ShadowConfig* config) {
    if (!config || !Shadow_IsVisible(config)) {
        return original_bounds;
    }
    
    Rectangle shadow_bounds = {
        original_bounds.x + config->offset.x,
        original_bounds.y + config->offset.y,
        original_bounds.width,
        original_bounds.height
    };
    
    // Expand for spread and blur
    float expansion = config->spread + config->blur_radius;
    shadow_bounds = ExpandRectangle(shadow_bounds, expansion);
    
    // Return union of original and shadow bounds
    float min_x = fminf(original_bounds.x, shadow_bounds.x);
    float min_y = fminf(original_bounds.y, shadow_bounds.y);
    float max_x = fmaxf(original_bounds.x + original_bounds.width, 
                       shadow_bounds.x + shadow_bounds.width);
    float max_y = fmaxf(original_bounds.y + original_bounds.height, 
                       shadow_bounds.y + shadow_bounds.height);
    
    return (Rectangle){min_x, min_y, max_x - min_x, max_y - min_y};
}