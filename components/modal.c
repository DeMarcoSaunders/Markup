#include "modal.h"
#include "theme.h"
#include "blur_effect.h"
#include "shadow.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper function to get modal size based on preset
static Vector2 GetModalSize(ModalSize size) {
    ModalProperties* props = &AppTheme.modal_props;
    float screen_width = (float)GetScreenWidth();
    float screen_height = (float)GetScreenHeight();
    
    switch (size) {
        case MODAL_SIZE_SMALL:
            return (Vector2){
                fminf(400.0f, screen_width * 0.6f),
                fminf(300.0f, screen_height * 0.5f)
            };
        case MODAL_SIZE_MEDIUM:
            return (Vector2){
                fminf(600.0f, screen_width * 0.7f),
                fminf(450.0f, screen_height * 0.6f)
            };
        case MODAL_SIZE_LARGE:
            return (Vector2){
                fminf(800.0f, screen_width * 0.8f),
                fminf(600.0f, screen_height * 0.7f)
            };
        case MODAL_SIZE_AUTO:
        default:
            return (Vector2){
                fmaxf(props->min_width, fminf(500.0f, screen_width * props->max_width_percent)),
                fmaxf(props->min_height, fminf(400.0f, screen_height * props->max_height_percent))
            };
    }
}

// Helper function to center modal on screen
static Rectangle CenterModal(Vector2 size) {
    float screen_width = (float)GetScreenWidth();
    float screen_height = (float)GetScreenHeight();
    
    return (Rectangle){
        (screen_width - size.x) * 0.5f,
        (screen_height - size.y) * 0.5f,
        size.x,
        size.y
    };
}

// Helper function for smooth animation easing
static float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

Modal Modal_Create(const char* title, ModalSize size) {
    Modal modal = {0};
    
    Vector2 modal_size = GetModalSize(size);
    modal.bounds = CenterModal(modal_size);
    modal.size_preset = size;
    
    // Set title
    if (title) {
        size_t title_len = strlen(title) + 1;
        modal.title = (char*)malloc(title_len);
        strcpy(modal.title, title);
    }
    
    // Initialize properties from theme
    ModalProperties* props = &AppTheme.modal_props;
    modal.close_on_overlay_click = props->close_on_overlay_click;
    modal.show_close_button = props->show_close_button;
    modal.use_blur_effect = props->use_blur_effect;
    
    // Initialize state
    modal.is_visible = false;
    modal.is_closing = false;
    modal.animation_progress = 0.0f;
    modal.target_progress = 0.0f;
    modal.is_dragging = false;
    modal.drag_offset = (Vector2){0, 0};
    modal.close_button_hovered = false;
    modal.close_button_pressed = false;
    modal.draw_content = NULL;
    modal.user_data = NULL;
    
    return modal;
}

Modal Modal_CreateCustom(const char* title, float width, float height) {
    Modal modal = Modal_Create(title, MODAL_SIZE_CUSTOM);
    
    ModalProperties* props = &AppTheme.modal_props;
    float screen_width = (float)GetScreenWidth();
    float screen_height = (float)GetScreenHeight();
    
    // Clamp to screen bounds and minimum size
    width = fmaxf(props->min_width, fminf(width, screen_width * props->max_width_percent));
    height = fmaxf(props->min_height, fminf(height, screen_height * props->max_height_percent));
    
    modal.bounds = CenterModal((Vector2){width, height});
    
    return modal;
}

void Modal_Update(Modal* modal) {
    if (!modal->is_visible && modal->animation_progress <= 0.0f) {
        return;
    }
    
    // Update animation
    float animation_speed = 8.0f * GetFrameTime();
    if (modal->animation_progress < modal->target_progress) {
        modal->animation_progress = fminf(modal->target_progress, modal->animation_progress + animation_speed);
    } else if (modal->animation_progress > modal->target_progress) {
        modal->animation_progress = fmaxf(modal->target_progress, modal->animation_progress - animation_speed);
        
        // If closing animation finished, hide modal
        if (modal->is_closing && modal->animation_progress <= 0.0f) {
            modal->is_visible = false;
            modal->is_closing = false;
        }
    }
    
    if (!modal->is_visible) return;
    
    Vector2 mouse_pos = GetMousePosition();
    ModalProperties* props = &AppTheme.modal_props;
    
    // Update close button state
    if (modal->show_close_button) {
        Rectangle close_button_rect = {
            modal->bounds.x + modal->bounds.width - props->close_button_size - 8.0f,
            modal->bounds.y + 8.0f,
            props->close_button_size,
            props->close_button_size
        };
        
        modal->close_button_hovered = CheckCollisionPointRec(mouse_pos, close_button_rect);
        
        if (modal->close_button_hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            modal->close_button_pressed = true;
        }
        
        if (modal->close_button_pressed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (modal->close_button_hovered) {
                Modal_Close(modal);
            }
            modal->close_button_pressed = false;
        }
    }
    
    // Handle dragging (title bar area)
    Rectangle title_bar = {
        modal->bounds.x,
        modal->bounds.y,
        modal->bounds.width,
        40.0f // Title bar height
    };
    
    bool mouse_over_title = CheckCollisionPointRec(mouse_pos, title_bar);
    
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse_over_title && !modal->close_button_hovered) {
        modal->is_dragging = true;
        modal->drag_offset.x = mouse_pos.x - modal->bounds.x;
        modal->drag_offset.y = mouse_pos.y - modal->bounds.y;
    }
    
    if (modal->is_dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float new_x = mouse_pos.x - modal->drag_offset.x;
            float new_y = mouse_pos.y - modal->drag_offset.y;
            
            // Keep modal on screen
            float screen_width = (float)GetScreenWidth();
            float screen_height = (float)GetScreenHeight();
            
            new_x = fmaxf(0.0f, fminf(new_x, screen_width - modal->bounds.width));
            new_y = fmaxf(0.0f, fminf(new_y, screen_height - modal->bounds.height));
            
            modal->bounds.x = new_x;
            modal->bounds.y = new_y;
        } else {
            modal->is_dragging = false;
        }
    }
    
    // Handle overlay click to close
    if (modal->close_on_overlay_click && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (!CheckCollisionPointRec(mouse_pos, modal->bounds)) {
            Modal_Close(modal);
        }
    }
    
    // Handle ESC key to close
    if (IsKeyPressed(KEY_ESCAPE)) {
        Modal_Close(modal);
    }
    
    // Update content bounds
    ComponentStyle* style = Theme_GetComponentStyle(COMPONENT_MODAL);
    if (style) {
        float title_height = modal->title ? 40.0f : 0.0f;
        modal->content_bounds = (Rectangle){
            modal->bounds.x + style->padding.left,
            modal->bounds.y + title_height + style->padding.top,
            modal->bounds.width - style->padding.left - style->padding.right,
            modal->bounds.height - title_height - style->padding.top - style->padding.bottom
        };
    }
}

void Modal_Draw(Modal* modal) {
    if (!modal->is_visible && modal->animation_progress <= 0.0f) {
        return;
    }
    
    ComponentStyle* style = Theme_GetComponentStyle(COMPONENT_MODAL);
    ModalProperties* props = &AppTheme.modal_props;
    
    if (!style) return;
    
    float ease_progress = EaseOutCubic(modal->animation_progress);
    
    // Draw simple overlay (blur effect should be handled externally now)
    Color overlay_color = (Color){0, 0, 0, (unsigned char)(255 * props->overlay_opacity * ease_progress)};
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), overlay_color);
    
    if (ease_progress <= 0.0f) return;
    
    // Scale and fade modal based on animation progress
    Rectangle animated_bounds = modal->bounds;
    float scale = 0.8f + (0.2f * ease_progress);
    float width_diff = animated_bounds.width * (1.0f - scale);
    float height_diff = animated_bounds.height * (1.0f - scale);
    
    animated_bounds.x += width_diff * 0.5f;
    animated_bounds.y += height_diff * 0.5f;
    animated_bounds.width *= scale;
    animated_bounds.height *= scale;
    
    ColorPalette* colors = &style->colors[STATE_DEFAULT];
    
    // Draw modal shadow
    if (style->shadow_blur > 0.0f) {
        Rectangle shadow_rect = {
            animated_bounds.x + style->shadow_offset.x,
            animated_bounds.y + style->shadow_offset.y,
            animated_bounds.width,
            animated_bounds.height
        };
        
        Color shadow_color = colors->shadow;
        shadow_color.a = (unsigned char)(shadow_color.a * ease_progress);
        
        DrawRectangleRounded(shadow_rect, 
                           style->border_radius.top_left / shadow_rect.height, 
                           8, shadow_color);
    }
    
    // Draw modal background
    Color bg_color = colors->background;
    bg_color.a = (unsigned char)(bg_color.a * ease_progress);
    DrawRectangleRounded(animated_bounds, 
                        style->border_radius.top_left / animated_bounds.height, 
                        8, bg_color);
    
    // Draw modal border
    if (style->border_width.top > 0.0f) {
        Color border_color = colors->border;
        border_color.a = (unsigned char)(border_color.a * ease_progress);
        DrawRectangleRoundedLines(animated_bounds, 
                                 style->border_radius.top_left / animated_bounds.height, 
                                 8, border_color);
    }
    
    // Draw title bar
    if (modal->title) {
        Rectangle title_rect = {
            animated_bounds.x,
            animated_bounds.y,
            animated_bounds.width,
            40.0f
        };
        
        // Draw title text
        Color text_color = colors->text;
        text_color.a = (unsigned char)(text_color.a * ease_progress);
        
        Vector2 title_pos = {
            title_rect.x + style->padding.left,
            title_rect.y + (title_rect.height - style->font_size) * 0.5f
        };
        
        DrawTextEx(GetFontDefault(), modal->title, title_pos, style->font_size, 1.0f, text_color);
        
        // Draw title separator line
        Color separator_color = colors->border;
        separator_color.a = (unsigned char)(separator_color.a * ease_progress * 0.5f);
        DrawLine((int)(title_rect.x + style->padding.left), 
                (int)(title_rect.y + title_rect.height - 1),
                (int)(title_rect.x + title_rect.width - style->padding.right), 
                (int)(title_rect.y + title_rect.height - 1),
                separator_color);
    }
    
    // Draw close button
    if (modal->show_close_button) {
        Rectangle close_button_rect = {
            animated_bounds.x + animated_bounds.width - props->close_button_size - 8.0f,
            animated_bounds.y + 8.0f,
            props->close_button_size,
            props->close_button_size
        };
        
        Color close_color = modal->close_button_hovered ? 
                           (Color){220, 38, 38, (unsigned char)(255 * ease_progress)} : // Red on hover
                           (Color){156, 163, 175, (unsigned char)(255 * ease_progress)}; // Gray default
        
        if (modal->close_button_pressed) {
            close_color = (Color){185, 28, 28, (unsigned char)(255 * ease_progress)}; // Darker red when pressed
        }
        
        // Draw close button background
        DrawRectangleRounded(close_button_rect, 0.3f, 4, close_color);
        
        // Draw X symbol
        Color x_color = (Color){255, 255, 255, (unsigned char)(255 * ease_progress)};
        float center_x = close_button_rect.x + close_button_rect.width * 0.5f;
        float center_y = close_button_rect.y + close_button_rect.height * 0.5f;
        float size = close_button_rect.width * 0.3f;
        
        DrawLineEx((Vector2){center_x - size, center_y - size}, 
                  (Vector2){center_x + size, center_y + size}, 2.0f, x_color);
        DrawLineEx((Vector2){center_x + size, center_y - size}, 
                  (Vector2){center_x - size, center_y + size}, 2.0f, x_color);
    }
    
    // Draw content using callback if provided
    if (modal->draw_content && ease_progress > 0.0f) {
        Rectangle content_area = modal->content_bounds;
        
        // Scale content area with animation
        float content_scale = scale;
        float content_width_diff = content_area.width * (1.0f - content_scale);
        float content_height_diff = content_area.height * (1.0f - content_scale);
        
        content_area.x += content_width_diff * 0.5f;
        content_area.y += content_height_diff * 0.5f;
        content_area.width *= content_scale;
        content_area.height *= content_scale;
        
        // Begin scissor mode for content clipping
        BeginScissorMode((int)content_area.x, (int)content_area.y, 
                        (int)content_area.width, (int)content_area.height);
        
        modal->draw_content(content_area, modal->user_data);
        
        EndScissorMode();
    }
}

void Modal_Show(Modal* modal) {
    modal->is_visible = true;
    modal->is_closing = false;
    modal->target_progress = 1.0f;
}

void Modal_Hide(Modal* modal) {
    modal->target_progress = 0.0f;
    modal->is_closing = true;
}

void Modal_Close(Modal* modal) {
    Modal_Hide(modal);
}

bool Modal_IsVisible(Modal* modal) {
    return modal->is_visible;
}

void Modal_SetTitle(Modal* modal, const char* title) {
    if (modal->title) {
        free(modal->title);
        modal->title = NULL;
    }
    
    if (title) {
        size_t title_len = strlen(title) + 1;
        modal->title = (char*)malloc(title_len);
        strcpy(modal->title, title);
    }
}

void Modal_SetSize(Modal* modal, ModalSize size) {
    modal->size_preset = size;
    Vector2 modal_size = GetModalSize(size);
    modal->bounds = CenterModal(modal_size);
}

void Modal_SetCustomSize(Modal* modal, float width, float height) {
    modal->size_preset = MODAL_SIZE_CUSTOM;
    
    ModalProperties* props = &AppTheme.modal_props;
    float screen_width = (float)GetScreenWidth();
    float screen_height = (float)GetScreenHeight();
    
    width = fmaxf(props->min_width, fminf(width, screen_width * props->max_width_percent));
    height = fmaxf(props->min_height, fminf(height, screen_height * props->max_height_percent));
    
    modal->bounds = CenterModal((Vector2){width, height});
}

void Modal_SetCloseOnOverlayClick(Modal* modal, bool enabled) {
    modal->close_on_overlay_click = enabled;
}

void Modal_SetShowCloseButton(Modal* modal, bool show) {
    modal->show_close_button = show;
}

void Modal_SetContentCallback(Modal* modal, void (*callback)(Rectangle, void*), void* user_data) {
    modal->draw_content = callback;
    modal->user_data = user_data;
}

Rectangle Modal_GetContentArea(Modal* modal) {
    return modal->content_bounds;
}

void Modal_DrawWithBackground(Modal* modal, void (*draw_background)(void)) {
    if (!modal->is_visible && modal->animation_progress <= 0.0f) {
        return;
    }
    
    ComponentStyle* style = Theme_GetComponentStyle(COMPONENT_MODAL);
    ModalProperties* props = &AppTheme.modal_props;
    
    if (!style) return;
    
    float ease_progress = EaseOutCubic(modal->animation_progress);
    
    // Draw backdrop with optional blur effect
    if (modal->use_blur_effect && BlurEffect_IsReady() && ease_progress > 0.0f && draw_background) {
        // Capture the background content
        BlurEffect_BeginCapture();
        draw_background(); // Draw the background content
        BlurEffect_EndCapture();
        
        // Apply blur effect
        BlurEffect_ApplyBlur(props->blur_strength * ease_progress);
        
        // Draw the blurred background
        BlurEffect_DrawBlurred();
        
        // Add a subtle overlay on top of blur
        Color overlay_color = (Color){0, 0, 0, (unsigned char)(255 * props->overlay_opacity * 0.3f * ease_progress)};
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), overlay_color);
    } else {
        // Draw background normally, then overlay
        if (draw_background) {
            draw_background();
        }
        Color overlay_color = (Color){0, 0, 0, (unsigned char)(255 * props->overlay_opacity * ease_progress)};
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), overlay_color);
    }
    
    // Draw the modal itself (rest of the original Modal_Draw code)
    if (ease_progress <= 0.0f) return;
    
    // Scale and fade modal based on animation progress
    Rectangle animated_bounds = modal->bounds;
    float scale = 0.8f + (0.2f * ease_progress);
    float width_diff = animated_bounds.width * (1.0f - scale);
    float height_diff = animated_bounds.height * (1.0f - scale);
    
    animated_bounds.x += width_diff * 0.5f;
    animated_bounds.y += height_diff * 0.5f;
    animated_bounds.width *= scale;
    animated_bounds.height *= scale;
    
    ColorPalette* colors = &style->colors[STATE_DEFAULT];
    
    // Draw modal shadow
    if (style->shadow_blur > 0.0f) {
        Rectangle shadow_rect = {
            animated_bounds.x + style->shadow_offset.x,
            animated_bounds.y + style->shadow_offset.y,
            animated_bounds.width,
            animated_bounds.height
        };
        
        Color shadow_color = colors->shadow;
        shadow_color.a = (unsigned char)(shadow_color.a * ease_progress);
        
        DrawRectangleRounded(shadow_rect, 
                           style->border_radius.top_left / shadow_rect.height, 
                           8, shadow_color);
    }
    
    // Draw modal background
    Color bg_color = colors->background;
    bg_color.a = (unsigned char)(bg_color.a * ease_progress);
    DrawRectangleRounded(animated_bounds, 
                        style->border_radius.top_left / animated_bounds.height, 
                        8, bg_color);
    
    // Draw modal border
    if (style->border_width.top > 0.0f) {
        Color border_color = colors->border;
        border_color.a = (unsigned char)(border_color.a * ease_progress);
        DrawRectangleRoundedLines(animated_bounds, 
                                 style->border_radius.top_left / animated_bounds.height, 
                                 8, border_color);
    }
    
    // Draw title bar
    if (modal->title) {
        Rectangle title_rect = {
            animated_bounds.x,
            animated_bounds.y,
            animated_bounds.width,
            40.0f
        };
        
        // Draw title text
        Color text_color = colors->text;
        text_color.a = (unsigned char)(text_color.a * ease_progress);
        
        Vector2 title_pos = {
            title_rect.x + style->padding.left,
            title_rect.y + (title_rect.height - style->font_size) * 0.5f
        };
        
        DrawTextEx(GetFontDefault(), modal->title, title_pos, style->font_size, 1.0f, text_color);
        
        // Draw title separator line
        Color separator_color = colors->border;
        separator_color.a = (unsigned char)(separator_color.a * ease_progress * 0.5f);
        DrawLine((int)(title_rect.x + style->padding.left), 
                (int)(title_rect.y + title_rect.height - 1),
                (int)(title_rect.x + title_rect.width - style->padding.right), 
                (int)(title_rect.y + title_rect.height - 1),
                separator_color);
    }
    
    // Draw close button
    if (modal->show_close_button) {
        Rectangle close_button_rect = {
            animated_bounds.x + animated_bounds.width - props->close_button_size - 8.0f,
            animated_bounds.y + 8.0f,
            props->close_button_size,
            props->close_button_size
        };
        
        Color close_color = modal->close_button_hovered ? 
                           (Color){220, 38, 38, (unsigned char)(255 * ease_progress)} : // Red on hover
                           (Color){156, 163, 175, (unsigned char)(255 * ease_progress)}; // Gray default
        
        if (modal->close_button_pressed) {
            close_color = (Color){185, 28, 28, (unsigned char)(255 * ease_progress)}; // Darker red when pressed
        }
        
        // Draw close button background
        DrawRectangleRounded(close_button_rect, 0.3f, 4, close_color);
        
        // Draw X symbol
        Color x_color = (Color){255, 255, 255, (unsigned char)(255 * ease_progress)};
        float center_x = close_button_rect.x + close_button_rect.width * 0.5f;
        float center_y = close_button_rect.y + close_button_rect.height * 0.5f;
        float size = close_button_rect.width * 0.3f;
        
        DrawLineEx((Vector2){center_x - size, center_y - size}, 
                  (Vector2){center_x + size, center_y + size}, 2.0f, x_color);
        DrawLineEx((Vector2){center_x + size, center_y - size}, 
                  (Vector2){center_x - size, center_y + size}, 2.0f, x_color);
    }
    
    // Draw content using callback if provided
    if (modal->draw_content && ease_progress > 0.0f) {
        Rectangle content_area = modal->content_bounds;
        
        // Scale content area with animation
        float content_scale = scale;
        float content_width_diff = content_area.width * (1.0f - content_scale);
        float content_height_diff = content_area.height * (1.0f - content_scale);
        
        content_area.x += content_width_diff * 0.5f;
        content_area.y += content_height_diff * 0.5f;
        content_area.width *= content_scale;
        content_area.height *= content_scale;
        
        // Begin scissor mode for content clipping
        BeginScissorMode((int)content_area.x, (int)content_area.y, 
                        (int)content_area.width, (int)content_area.height);
        
        modal->draw_content(content_area, modal->user_data);
        
        EndScissorMode();
    }
}

void Modal_SetBlurEffect(Modal* modal, bool enabled) {
    modal->use_blur_effect = enabled;
}

void Modal_Destroy(Modal* modal) {
    if (modal->title) {
        free(modal->title);
        modal->title = NULL;
    }
}