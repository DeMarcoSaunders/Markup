#include "spinner.h"
#include "error_handling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

Spinner Spinner_Create(Rectangle rect, SpinnerStyle style) {
    Spinner spinner = {0};
    
    // Initialize base component
    if (!ComponentBase_Init(&spinner.base, COMPONENT_TYPE_CUSTOM, rect)) {
        SET_ERROR(ERROR_COMPONENT_NOT_FOUND, "Failed to initialize spinner base");
        return spinner;
    }
    
    // Set default values
    spinner.style = style;
    spinner.size = SPINNER_SIZE_MEDIUM;
    spinner.custom_size = 40.0f;
    
    // Colors
    spinner.primary_color = (Color){59, 130, 246, 255};
    spinner.secondary_color = (Color){147, 51, 234, 255};
    spinner.background_color = (Color){0, 0, 0, 0}; // Transparent
    
    // Animation properties
    spinner.rotation_speed = 180.0f; // degrees per second
    spinner.animation_speed = 1.0f;
    spinner.current_rotation = 0.0f;
    spinner.animation_time = 0.0f;
    
    // Style-specific properties
    spinner.dot_count = 8;
    spinner.bar_count = 8;
    spinner.bar_width = 4.0f;
    spinner.bar_gap = 2.0f;
    spinner.ring_thickness = 4.0f;
    spinner.pulse_scale = 1.0f;
    spinner.wave_offset = 0.0f;
    
    // State
    spinner.is_spinning = true;
    spinner.is_visible = true;
    
    // Text display
    spinner.show_text = false;
    strcpy(spinner.text, "Loading...");
    spinner.font_size = 14;
    spinner.text_color = (Color){50, 50, 50, 255};
    spinner.text_offset = 30.0f;
    
    // Callbacks
    spinner.on_spin_start = NULL;
    spinner.on_spin_stop = NULL;
    
    return spinner;
}

bool Spinner_Init(Spinner* spinner, Rectangle rect, SpinnerStyle style) {
    RETURN_IF_NULL(spinner);
    RETURN_IF_INVALID_RECT(rect);
    
    *spinner = Spinner_Create(rect, style);
    return !Error_HasError();
}

void Spinner_Update(Spinner* spinner) {
    if (!spinner || !spinner->base.is_visible || !spinner->is_spinning) return;
    
    float delta_time = GetFrameTime();
    spinner->animation_time += delta_time * spinner->animation_speed;
    
    // Update rotation
    spinner->current_rotation += spinner->rotation_speed * delta_time;
    if (spinner->current_rotation >= 360.0f) {
        spinner->current_rotation -= 360.0f;
    }
    
    // Update style-specific animations
    switch (spinner->style) {
        case SPINNER_STYLE_PULSE:
            spinner->pulse_scale = 0.5f + 0.5f * sinf(spinner->animation_time * 2.0f);
            break;
        case SPINNER_STYLE_WAVE:
            spinner->wave_offset = fmodf(spinner->animation_time * 100.0f, 200.0f);
            break;
        case SPINNER_STYLE_BOUNCE:
            // Bounce animation is handled in draw function
            break;
        default:
            break;
    }
}

void Spinner_Draw(const Spinner* spinner) {
    if (!spinner || !spinner->base.is_visible) return;
    
    Vector2 center = {spinner->base.rect.x + spinner->base.rect.width * 0.5f, 
                     spinner->base.rect.y + spinner->base.rect.height * 0.5f};
    float size = Spinner_GetSize(spinner);
    
    // Draw background if needed
    if (spinner->background_color.a > 0) {
        DrawCircle(center.x, center.y, size * 0.5f, spinner->background_color);
    }
    
    // Draw spinner based on style
    switch (spinner->style) {
        case SPINNER_STYLE_CIRCULAR:
            Spinner_DrawCircular(spinner, center, size);
            break;
        case SPINNER_STYLE_DOTS:
            Spinner_DrawDots(spinner, center, size);
            break;
        case SPINNER_STYLE_PULSE:
            Spinner_DrawPulse(spinner, center, size);
            break;
        case SPINNER_STYLE_WAVE:
            Spinner_DrawWave(spinner, center, size);
            break;
        case SPINNER_STYLE_BARS:
            Spinner_DrawBars(spinner, center, size);
            break;
        case SPINNER_STYLE_RING:
            Spinner_DrawRing(spinner, center, size);
            break;
        case SPINNER_STYLE_SPIRAL:
            Spinner_DrawSpiral(spinner, center, size);
            break;
        case SPINNER_STYLE_BOUNCE:
            Spinner_DrawBounce(spinner, center, size);
            break;
    }
    
    // Draw text
    if (spinner->show_text) {
        Spinner_DrawText(spinner, center, size);
    }
}

float Spinner_GetSize(const Spinner* spinner) {
    switch (spinner->size) {
        case SPINNER_SIZE_SMALL: return 20.0f;
        case SPINNER_SIZE_MEDIUM: return 40.0f;
        case SPINNER_SIZE_LARGE: return 60.0f;
        case SPINNER_SIZE_CUSTOM: return spinner->custom_size;
        default: return 40.0f;
    }
}

void Spinner_DrawCircular(const Spinner* spinner, Vector2 center, float size) {
    float radius = size * 0.4f;
    float thickness = size * 0.1f;
    
    // Draw rotating arc
    float start_angle = spinner->current_rotation;
    float end_angle = start_angle + 270.0f;
    
    DrawCircleSectorLines(center, radius, start_angle, end_angle, 0, thickness, spinner->primary_color);
}

void Spinner_DrawDots(const Spinner* spinner, Vector2 center, float size) {
    float radius = size * 0.4f;
    float dot_size = size * 0.08f;
    
    for (int i = 0; i < spinner->dot_count; i++) {
        float angle = spinner->current_rotation + (360.0f / spinner->dot_count) * i;
        float alpha = 1.0f - (float)i / spinner->dot_count;
        
        Vector2 pos = {
            center.x + cosf(angle * DEG2RAD) * radius,
            center.y + sinf(angle * DEG2RAD) * radius
        };
        
        Color color = ColorAlpha(spinner->primary_color, alpha);
        DrawCircle(pos.x, pos.y, dot_size, color);
    }
}

void Spinner_DrawPulse(const Spinner* spinner, Vector2 center, float size) {
    float radius = size * 0.4f * spinner->pulse_scale;
    DrawCircle(center.x, center.y, radius, spinner->primary_color);
}

void Spinner_DrawWave(const Spinner* spinner, Vector2 center, float size) {
    float radius = size * 0.4f;
    int segments = 12;
    
    for (int i = 0; i < segments; i++) {
        float angle = (360.0f / segments) * i;
        float wave = sinf(spinner->animation_time * 2.0f + (float)i * 0.5f);
        float current_radius = radius + wave * 10.0f;
        
        Vector2 pos = {
            center.x + cosf(angle * DEG2RAD) * current_radius,
            center.y + sinf(angle * DEG2RAD) * current_radius
        };
        
        float alpha = 0.3f + 0.7f * (wave + 1.0f) * 0.5f;
        Color color = ColorAlpha(spinner->primary_color, alpha);
        DrawCircle(pos.x, pos.y, 3.0f, color);
    }
}

void Spinner_DrawBars(const Spinner* spinner, Vector2 center, float size) {
    float radius = size * 0.4f;
    float bar_length = size * 0.3f;
    
    for (int i = 0; i < spinner->bar_count; i++) {
        float angle = spinner->current_rotation + (360.0f / spinner->bar_count) * i;
        float alpha = 1.0f - (float)i / spinner->bar_count;
        
        Vector2 start = {
            center.x + cosf(angle * DEG2RAD) * (radius - bar_length * 0.5f),
            center.y + sinf(angle * DEG2RAD) * (radius - bar_length * 0.5f)
        };
        
        Vector2 end = {
            center.x + cosf(angle * DEG2RAD) * (radius + bar_length * 0.5f),
            center.y + sinf(angle * DEG2RAD) * (radius + bar_length * 0.5f)
        };
        
        Color color = ColorAlpha(spinner->primary_color, alpha);
        DrawLineEx(start, end, spinner->bar_width, color);
    }
}

void Spinner_DrawRing(const Spinner* spinner, Vector2 center, float size) {
    float radius = size * 0.4f;
    
    // Draw background ring
    DrawCircleSector(center, radius, 0, 360, 0, ColorAlpha(spinner->secondary_color, 0.2f));
    
    // Draw progress ring
    float progress = fmodf(spinner->animation_time * 0.5f, 1.0f);
    float start_angle = -90.0f;
    float end_angle = start_angle + 360.0f * progress;
    
    DrawCircleSector(center, radius, start_angle, end_angle, 0, spinner->primary_color);
}

void Spinner_DrawSpiral(const Spinner* spinner, Vector2 center, float size) {
    float max_radius = size * 0.4f;
    int segments = 50;
    
    for (int i = 0; i < segments; i++) {
        float t = (float)i / segments;
        float angle = spinner->current_rotation + t * 720.0f; // 2 full rotations
        float radius = t * max_radius;
        
        Vector2 pos = {
            center.x + cosf(angle * DEG2RAD) * radius,
            center.y + sinf(angle * DEG2RAD) * radius
        };
        
        float alpha = 1.0f - t;
        Color color = ColorAlpha(spinner->primary_color, alpha);
        DrawCircle(pos.x, pos.y, 2.0f, color);
    }
}

void Spinner_DrawBounce(const Spinner* spinner, Vector2 center, float size) {
    float radius = size * 0.4f;
    int dots = 3;
    
    for (int i = 0; i < dots; i++) {
        float t = (float)i / dots;
        float bounce = sinf(spinner->animation_time * 3.0f + t * PI);
        float y_offset = bounce * 15.0f;
        
        Vector2 pos = {
            center.x + (t - 0.5f) * radius,
            center.y + y_offset
        };
        
        float alpha = 1.0f - t * 0.5f;
        Color color = ColorAlpha(spinner->primary_color, alpha);
        DrawCircle(pos.x, pos.y, 6.0f, color);
    }
}

void Spinner_DrawText(const Spinner* spinner, Vector2 center, float size) {
    Vector2 text_size = MeasureTextEx(GetFontDefault(), spinner->text, spinner->font_size, 1.0f);
    Vector2 text_pos = {
        center.x - text_size.x * 0.5f,
        center.y + size * 0.5f + spinner->text_offset
    };
    
    DrawTextEx(GetFontDefault(), spinner->text, text_pos, spinner->font_size, 1.0f, spinner->text_color);
}

void Spinner_Destroy(Spinner* spinner) {
    if (!spinner) return;
    ComponentBase_Destroy(&spinner->base);
}

// Style Configuration
void Spinner_SetStyle(Spinner* spinner, SpinnerStyle style) {
    if (!spinner) return;
    spinner->style = style;
}

void Spinner_SetSize(Spinner* spinner, SpinnerSize size) {
    if (!spinner) return;
    spinner->size = size;
}

void Spinner_SetCustomSize(Spinner* spinner, float size) {
    if (!spinner) return;
    spinner->custom_size = fmaxf(10.0f, size);
}

// Visual Customization
void Spinner_SetColors(Spinner* spinner, Color primary, Color secondary, Color background) {
    if (!spinner) return;
    spinner->primary_color = primary;
    spinner->secondary_color = secondary;
    spinner->background_color = background;
}

void Spinner_SetPrimaryColor(Spinner* spinner, Color color) {
    if (!spinner) return;
    spinner->primary_color = color;
}

void Spinner_SetSecondaryColor(Spinner* spinner, Color color) {
    if (!spinner) return;
    spinner->secondary_color = color;
}

void Spinner_SetBackgroundColor(Spinner* spinner, Color color) {
    if (!spinner) return;
    spinner->background_color = color;
}

// Animation Control
void Spinner_SetRotationSpeed(Spinner* spinner, float speed) {
    if (!spinner) return;
    spinner->rotation_speed = fmaxf(0.0f, speed);
}

void Spinner_SetAnimationSpeed(Spinner* spinner, float speed) {
    if (!spinner) return;
    spinner->animation_speed = fmaxf(0.1f, speed);
}

void Spinner_Start(Spinner* spinner) {
    if (!spinner) return;
    spinner->is_spinning = true;
    if (spinner->on_spin_start) {
        spinner->on_spin_start(spinner);
    }
}

void Spinner_Stop(Spinner* spinner) {
    if (!spinner) return;
    spinner->is_spinning = false;
    if (spinner->on_spin_stop) {
        spinner->on_spin_stop(spinner);
    }
}

void Spinner_Pause(Spinner* spinner) {
    if (!spinner) return;
    spinner->is_spinning = false;
}

void Spinner_Resume(Spinner* spinner) {
    if (!spinner) return;
    spinner->is_spinning = true;
}

// Style-specific Configuration
void Spinner_SetDotCount(Spinner* spinner, int count) {
    if (!spinner) return;
    spinner->dot_count = fmaxf(3, count);
}

void Spinner_SetBarCount(Spinner* spinner, int count) {
    if (!spinner) return;
    spinner->bar_count = fmaxf(3, count);
}

void Spinner_SetBarWidth(Spinner* spinner, float width) {
    if (!spinner) return;
    spinner->bar_width = fmaxf(1.0f, width);
}

void Spinner_SetBarGap(Spinner* spinner, float gap) {
    if (!spinner) return;
    spinner->bar_gap = fmaxf(0.0f, gap);
}

void Spinner_SetRingThickness(Spinner* spinner, float thickness) {
    if (!spinner) return;
    spinner->ring_thickness = fmaxf(1.0f, thickness);
}

// Text Display
void Spinner_SetShowText(Spinner* spinner, bool show) {
    if (!spinner) return;
    spinner->show_text = show;
}

void Spinner_SetText(Spinner* spinner, const char* text) {
    if (!spinner || !text) return;
    strncpy(spinner->text, text, sizeof(spinner->text) - 1);
    spinner->text[sizeof(spinner->text) - 1] = '\0';
}

void Spinner_SetFontSize(Spinner* spinner, int size) {
    if (!spinner) return;
    spinner->font_size = fmaxf(8, size);
}

void Spinner_SetTextColor(Spinner* spinner, Color color) {
    if (!spinner) return;
    spinner->text_color = color;
}

void Spinner_SetTextOffset(Spinner* spinner, float offset) {
    if (!spinner) return;
    spinner->text_offset = offset;
}

// State Management
void Spinner_SetVisible(Spinner* spinner, bool visible) {
    if (!spinner) return;
    spinner->is_visible = visible;
    spinner->base.is_visible = visible;
}

void Spinner_SetSpinning(Spinner* spinner, bool spinning) {
    if (!spinner) return;
    spinner->is_spinning = spinning;
}

// Callbacks
void Spinner_SetOnStartCallback(Spinner* spinner, void (*callback)(Spinner* spinner)) {
    if (!spinner) return;
    spinner->on_spin_start = callback;
}

void Spinner_SetOnStopCallback(Spinner* spinner, void (*callback)(Spinner* spinner)) {
    if (!spinner) return;
    spinner->on_spin_stop = callback;
}

// Utility Functions
bool Spinner_IsSpinning(const Spinner* spinner) {
    return spinner && spinner->is_spinning;
}

bool Spinner_IsVisible(const Spinner* spinner) {
    return spinner && spinner->is_visible;
}

float Spinner_GetCurrentRotation(const Spinner* spinner) {
    return spinner ? spinner->current_rotation : 0.0f;
}

// Preset Configurations
void Spinner_SetPresetSmall(Spinner* spinner) {
    if (!spinner) return;
    spinner->size = SPINNER_SIZE_SMALL;
    spinner->font_size = 12;
    spinner->text_offset = 20.0f;
}

void Spinner_SetPresetMedium(Spinner* spinner) {
    if (!spinner) return;
    spinner->size = SPINNER_SIZE_MEDIUM;
    spinner->font_size = 14;
    spinner->text_offset = 30.0f;
}

void Spinner_SetPresetLarge(Spinner* spinner) {
    if (!spinner) return;
    spinner->size = SPINNER_SIZE_LARGE;
    spinner->font_size = 16;
    spinner->text_offset = 40.0f;
}

void Spinner_SetPresetFast(Spinner* spinner) {
    if (!spinner) return;
    spinner->rotation_speed = 360.0f;
    spinner->animation_speed = 2.0f;
}

void Spinner_SetPresetSlow(Spinner* spinner) {
    if (!spinner) return;
    spinner->rotation_speed = 90.0f;
    spinner->animation_speed = 0.5f;
}
