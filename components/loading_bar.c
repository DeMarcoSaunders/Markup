#include "loading_bar.h"
#include "error_handling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

LoadingBar LoadingBar_Create(Rectangle rect, LoadingBarStyle style) {
    LoadingBar bar = {0};
    
    // Initialize base component
    if (!ComponentBase_Init(&bar.base, COMPONENT_TYPE_CUSTOM, rect)) {
        SET_ERROR(ERROR_COMPONENT_NOT_FOUND, "Failed to initialize loading bar base");
        return bar;
    }
    
    // Set default values
    bar.progress = 0.0f;
    bar.target_progress = 0.0f;
    bar.min_value = 0.0f;
    bar.max_value = 100.0f;
    bar.current_value = 0.0f;
    
    // Style and animation
    bar.style = style;
    bar.direction = LOADING_BAR_DIRECTION_LEFT_TO_RIGHT;
    bar.animation = LOADING_BAR_ANIMATION_SMOOTH;
    bar.animation_speed = 2.0f;
    
    // Colors
    bar.background_color = (Color){240, 240, 240, 255};
    bar.progress_color = (Color){59, 130, 246, 255};
    bar.border_color = (Color){200, 200, 200, 255};
    bar.text_color = (Color){50, 50, 50, 255};
    
    // Dimensions
    bar.bar_thickness = 20.0f;
    bar.border_width = 1.0f;
    bar.corner_radius = 4.0f;
    
    // Animation properties
    bar.pulse_scale = 1.0f;
    bar.wave_offset = 0.0f;
    bar.bounce_height = 0.0f;
    
    // Text display
    bar.show_percentage = true;
    bar.show_value = false;
    bar.show_label = false;
    strcpy(bar.label, "");
    bar.font_size = 14;
    
    // Segmented properties
    bar.segment_count = 10;
    bar.segment_gap = 2.0f;
    
    // Gradient
    bar.gradient_start = (Color){59, 130, 246, 255};
    bar.gradient_end = (Color){147, 51, 234, 255};
    
    // State
    bar.is_indeterminate = false;
    bar.is_animated = true;
    bar.animation_time = 0.0f;
    
    // Callbacks
    bar.on_progress_complete = NULL;
    bar.on_progress_change = NULL;
    
    return bar;
}

bool LoadingBar_Init(LoadingBar* bar, Rectangle rect, LoadingBarStyle style) {
    RETURN_IF_NULL(bar);
    RETURN_IF_INVALID_RECT(rect);
    
    *bar = LoadingBar_Create(rect, style);
    return !Error_HasError();
}

void LoadingBar_Update(LoadingBar* bar) {
    if (!bar || !bar->base.is_visible) return;
    
    float delta_time = GetFrameTime();
    bar->animation_time += delta_time;
    
    // Update smooth animation
    if (bar->animation == LOADING_BAR_ANIMATION_SMOOTH && bar->is_animated) {
        float diff = bar->target_progress - bar->progress;
        if (fabsf(diff) > 0.001f) {
            bar->progress += diff * bar->animation_speed * delta_time;
            
            // Call change callback
            if (bar->on_progress_change) {
                bar->on_progress_change(bar, bar->progress - diff * bar->animation_speed * delta_time, bar->progress);
            }
        }
    }
    
    // Update indeterminate animation
    if (bar->is_indeterminate) {
        switch (bar->animation) {
            case LOADING_BAR_ANIMATION_PULSE:
                bar->pulse_scale = 0.8f + 0.2f * sinf(bar->animation_time * 3.0f);
                break;
            case LOADING_BAR_ANIMATION_WAVE:
                bar->wave_offset = fmodf(bar->animation_time * 100.0f, 200.0f);
                break;
            case LOADING_BAR_ANIMATION_BOUNCE:
                bar->bounce_height = 5.0f * sinf(bar->animation_time * 2.0f);
                break;
            default:
                break;
        }
    }
    
    // Check for completion
    if (bar->progress >= 1.0f && bar->on_progress_complete) {
        bar->on_progress_complete(bar);
    }
}

void LoadingBar_Draw(const LoadingBar* bar) {
    if (!bar || !bar->base.is_visible) return;
    
    Rectangle rect = bar->base.rect;
    Vector2 center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
    
    // Draw background
    DrawRectangleRec(rect, bar->background_color);
    
    // Draw border
    if (bar->border_width > 0) {
        DrawRectangleLinesEx(rect, bar->border_width, bar->border_color);
    }
    
    // Draw progress based on style
    switch (bar->style) {
        case LOADING_BAR_STYLE_LINEAR:
            LoadingBar_DrawLinear(bar);
            break;
        case LOADING_BAR_STYLE_CIRCULAR:
            LoadingBar_DrawCircular(bar);
            break;
        case LOADING_BAR_STYLE_PULSING:
            LoadingBar_DrawPulsing(bar);
            break;
        case LOADING_BAR_STYLE_GRADIENT:
            LoadingBar_DrawGradient(bar);
            break;
        case LOADING_BAR_STYLE_SEGMENTED:
            LoadingBar_DrawSegmented(bar);
            break;
    }
    
    // Draw text
    LoadingBar_DrawText(bar);
}

void LoadingBar_DrawLinear(const LoadingBar* bar) {
    Rectangle rect = bar->base.rect;
    float padding = bar->border_width + 2.0f;
    Rectangle progress_rect = {
        rect.x + padding,
        rect.y + padding,
        (rect.width - padding * 2) * bar->progress,
        rect.height - padding * 2
    };
    
    if (bar->corner_radius > 0) {
        DrawRectangleRounded(progress_rect, bar->corner_radius / rect.height, 10, bar->progress_color);
    } else {
        DrawRectangleRec(progress_rect, bar->progress_color);
    }
}

void LoadingBar_DrawCircular(const LoadingBar* bar) {
    Vector2 center = {bar->base.rect.x + bar->base.rect.width * 0.5f, 
                     bar->base.rect.y + bar->base.rect.height * 0.5f};
    float radius = fminf(bar->base.rect.width, bar->base.rect.height) * 0.4f;
    
    // Draw background circle
    DrawCircle(center.x, center.y, radius, bar->background_color);
    
    // Draw progress arc
    float start_angle = -90.0f;
    float end_angle = start_angle + 360.0f * bar->progress;
    
    DrawCircleSector(center, radius, start_angle, end_angle, 0, bar->progress_color);
}

void LoadingBar_DrawPulsing(const LoadingBar* bar) {
    Rectangle rect = bar->base.rect;
    float padding = bar->border_width + 2.0f;
    float width = (rect.width - padding * 2) * bar->progress * bar->pulse_scale;
    
    Rectangle progress_rect = {
        rect.x + padding,
        rect.y + padding,
        width,
        rect.height - padding * 2
    };
    
    DrawRectangleRec(progress_rect, bar->progress_color);
}

void LoadingBar_DrawGradient(const LoadingBar* bar) {
    Rectangle rect = bar->base.rect;
    float padding = bar->border_width + 2.0f;
    float progress_width = (rect.width - padding * 2) * bar->progress;
    
    // Draw gradient segments
    int segments = 20;
    float segment_width = progress_width / segments;
    
    for (int i = 0; i < segments; i++) {
        float t = (float)i / (segments - 1);
        Color color = ColorAlphaBlend(bar->gradient_start, bar->gradient_end, t);
        
        Rectangle segment_rect = {
            rect.x + padding + i * segment_width,
            rect.y + padding,
            segment_width,
            rect.height - padding * 2
        };
        
        DrawRectangleRec(segment_rect, color);
    }
}

void LoadingBar_DrawSegmented(const LoadingBar* bar) {
    Rectangle rect = bar->base.rect;
    float padding = bar->border_width + 2.0f;
    float available_width = rect.width - padding * 2;
    float segment_width = (available_width - (bar->segment_count - 1) * bar->segment_gap) / bar->segment_count;
    
    for (int i = 0; i < bar->segment_count; i++) {
        float segment_progress = (float)i / bar->segment_count;
        Color color = (segment_progress <= bar->progress) ? bar->progress_color : bar->background_color;
        
        Rectangle segment_rect = {
            rect.x + padding + i * (segment_width + bar->segment_gap),
            rect.y + padding,
            segment_width,
            rect.height - padding * 2
        };
        
        DrawRectangleRec(segment_rect, color);
    }
}

void LoadingBar_DrawText(const LoadingBar* bar) {
    if (!bar->show_percentage && !bar->show_value && !bar->show_label) return;
    
    Vector2 center = {bar->base.rect.x + bar->base.rect.width * 0.5f, 
                     bar->base.rect.y + bar->base.rect.height * 0.5f};
    char text[256] = "";
    
    if (bar->show_percentage) {
        sprintf(text, "%d%%", (int)(bar->progress * 100));
    } else if (bar->show_value) {
        sprintf(text, "%.1f", bar->current_value);
    } else if (bar->show_label) {
        strcpy(text, bar->label);
    }
    
    Vector2 text_size = MeasureTextEx(GetFontDefault(), text, bar->font_size, 1.0f);
    Vector2 text_pos = {center.x - text_size.x * 0.5f, center.y - text_size.y * 0.5f};
    
    DrawTextEx(GetFontDefault(), text, text_pos, bar->font_size, 1.0f, bar->text_color);
}

void LoadingBar_Destroy(LoadingBar* bar) {
    if (!bar) return;
    ComponentBase_Destroy(&bar->base);
}

// Progress Management
void LoadingBar_SetProgress(LoadingBar* bar, float progress) {
    if (!bar) return;
    
    float old_progress = bar->progress;
    bar->target_progress = fmaxf(0.0f, fminf(1.0f, progress));
    
    if (bar->animation == LOADING_BAR_ANIMATION_NONE) {
        bar->progress = bar->target_progress;
    }
    
    if (bar->on_progress_change) {
        bar->on_progress_change(bar, old_progress, bar->progress);
    }
}

void LoadingBar_SetValue(LoadingBar* bar, float value) {
    if (!bar) return;
    
    bar->current_value = fmaxf(bar->min_value, fminf(bar->max_value, value));
    float progress = (bar->current_value - bar->min_value) / (bar->max_value - bar->min_value);
    LoadingBar_SetProgress(bar, progress);
}

void LoadingBar_SetRange(LoadingBar* bar, float min_value, float max_value) {
    if (!bar) return;
    
    bar->min_value = min_value;
    bar->max_value = max_value;
    
    // Recalculate progress based on current value
    if (bar->max_value > bar->min_value) {
        float progress = (bar->current_value - bar->min_value) / (bar->max_value - bar->min_value);
        LoadingBar_SetProgress(bar, progress);
    }
}

float LoadingBar_GetProgress(const LoadingBar* bar) {
    return bar ? bar->progress : 0.0f;
}

float LoadingBar_GetValue(const LoadingBar* bar) {
    return bar ? bar->current_value : 0.0f;
}

void LoadingBar_Reset(LoadingBar* bar) {
    if (!bar) return;
    LoadingBar_SetProgress(bar, 0.0f);
    bar->animation_time = 0.0f;
}

// Style Configuration
void LoadingBar_SetStyle(LoadingBar* bar, LoadingBarStyle style) {
    if (!bar) return;
    bar->style = style;
}

void LoadingBar_SetDirection(LoadingBar* bar, LoadingBarDirection direction) {
    if (!bar) return;
    bar->direction = direction;
}

void LoadingBar_SetAnimation(LoadingBar* bar, LoadingBarAnimation animation) {
    if (!bar) return;
    bar->animation = animation;
}

void LoadingBar_SetAnimationSpeed(LoadingBar* bar, float speed) {
    if (!bar) return;
    bar->animation_speed = fmaxf(0.1f, speed);
}

// Visual Customization
void LoadingBar_SetColors(LoadingBar* bar, Color background, Color progress, Color border) {
    if (!bar) return;
    bar->background_color = background;
    bar->progress_color = progress;
    bar->border_color = border;
}

void LoadingBar_SetProgressColor(LoadingBar* bar, Color color) {
    if (!bar) return;
    bar->progress_color = color;
}

void LoadingBar_SetBackgroundColor(LoadingBar* bar, Color color) {
    if (!bar) return;
    bar->background_color = color;
}

void LoadingBar_SetBorderColor(LoadingBar* bar, Color color) {
    if (!bar) return;
    bar->border_color = color;
}

void LoadingBar_SetTextColor(LoadingBar* bar, Color color) {
    if (!bar) return;
    bar->text_color = color;
}

// Dimensions
void LoadingBar_SetThickness(LoadingBar* bar, float thickness) {
    if (!bar) return;
    bar->bar_thickness = fmaxf(1.0f, thickness);
}

void LoadingBar_SetBorderWidth(LoadingBar* bar, float width) {
    if (!bar) return;
    bar->border_width = fmaxf(0.0f, width);
}

void LoadingBar_SetCornerRadius(LoadingBar* bar, float radius) {
    if (!bar) return;
    bar->corner_radius = fmaxf(0.0f, radius);
}

// Text Display
void LoadingBar_SetShowPercentage(LoadingBar* bar, bool show) {
    if (!bar) return;
    bar->show_percentage = show;
}

void LoadingBar_SetShowValue(LoadingBar* bar, bool show) {
    if (!bar) return;
    bar->show_value = show;
}

void LoadingBar_SetShowLabel(LoadingBar* bar, bool show) {
    if (!bar) return;
    bar->show_label = show;
}

void LoadingBar_SetLabel(LoadingBar* bar, const char* label) {
    if (!bar || !label) return;
    strncpy(bar->label, label, sizeof(bar->label) - 1);
    bar->label[sizeof(bar->label) - 1] = '\0';
}

void LoadingBar_SetFontSize(LoadingBar* bar, int size) {
    if (!bar) return;
    bar->font_size = fmaxf(8, size);
}

// Segmented Bar
void LoadingBar_SetSegmentCount(LoadingBar* bar, int count) {
    if (!bar) return;
    bar->segment_count = fmaxf(2, count);
}

void LoadingBar_SetSegmentGap(LoadingBar* bar, float gap) {
    if (!bar) return;
    bar->segment_gap = fmaxf(0.0f, gap);
}

// Gradient
void LoadingBar_SetGradient(LoadingBar* bar, Color start, Color end) {
    if (!bar) return;
    bar->gradient_start = start;
    bar->gradient_end = end;
}

// State Management
void LoadingBar_SetIndeterminate(LoadingBar* bar, bool indeterminate) {
    if (!bar) return;
    bar->is_indeterminate = indeterminate;
}

void LoadingBar_SetAnimated(LoadingBar* bar, bool animated) {
    if (!bar) return;
    bar->is_animated = animated;
}

// Callbacks
void LoadingBar_SetOnCompleteCallback(LoadingBar* bar, void (*callback)(LoadingBar* bar)) {
    if (!bar) return;
    bar->on_progress_complete = callback;
}

void LoadingBar_SetOnChangeCallback(LoadingBar* bar, void (*callback)(LoadingBar* bar, float old_progress, float new_progress)) {
    if (!bar) return;
    bar->on_progress_change = callback;
}

// Utility Functions
bool LoadingBar_IsComplete(const LoadingBar* bar) {
    return bar && bar->progress >= 1.0f;
}

bool LoadingBar_IsIndeterminate(const LoadingBar* bar) {
    return bar && bar->is_indeterminate;
}

bool LoadingBar_IsAnimated(const LoadingBar* bar) {
    return bar && bar->is_animated;
}

// Animation Helpers
void LoadingBar_StartAnimation(LoadingBar* bar) {
    if (!bar) return;
    bar->is_animated = true;
}

void LoadingBar_StopAnimation(LoadingBar* bar) {
    if (!bar) return;
    bar->is_animated = false;
}

void LoadingBar_PauseAnimation(LoadingBar* bar) {
    if (!bar) return;
    bar->is_animated = false;
}

void LoadingBar_ResumeAnimation(LoadingBar* bar) {
    if (!bar) return;
    bar->is_animated = true;
}
