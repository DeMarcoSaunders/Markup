#include "slider.h"
#include "theme.h"
#include <math.h>
#include <stdio.h>

// Helper function to clamp a value between min and max
static float Clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Helper function to quantize value to step size
static float QuantizeToStep(float value, float step) {
    if (step <= 0.0f) return value;
    return roundf(value / step) * step;
}

// Helper function to get handle rectangle based on current value
static Rectangle GetHandleRect(Slider* slider) {
    SliderProperties* props = &AppTheme.slider_props;
    float handle_size = props->handle_size;
    
    if (slider->orientation == SLIDER_HORIZONTAL) {
        float track_width = slider->bounds.width - handle_size;
        float progress = (slider->value - slider->min_value) / (slider->max_value - slider->min_value);
        float handle_x = slider->bounds.x + (progress * track_width);
        float handle_y = slider->bounds.y + (slider->bounds.height - handle_size) * 0.5f;
        
        return (Rectangle){handle_x, handle_y, handle_size, handle_size};
    } else {
        float track_height = slider->bounds.height - handle_size;
        float progress = (slider->value - slider->min_value) / (slider->max_value - slider->min_value);
        float handle_x = slider->bounds.x + (slider->bounds.width - handle_size) * 0.5f;
        float handle_y = slider->bounds.y + ((1.0f - progress) * track_height); // Inverted for vertical
        
        return (Rectangle){handle_x, handle_y, handle_size, handle_size};
    }
}

// Helper function to get track rectangle
static Rectangle GetTrackRect(Slider* slider) {
    SliderProperties* props = &AppTheme.slider_props;
    float track_height = props->track_height;
    
    if (slider->orientation == SLIDER_HORIZONTAL) {
        float track_y = slider->bounds.y + (slider->bounds.height - track_height) * 0.5f;
        return (Rectangle){slider->bounds.x, track_y, slider->bounds.width, track_height};
    } else {
        float track_x = slider->bounds.x + (slider->bounds.width - track_height) * 0.5f;
        return (Rectangle){track_x, slider->bounds.y, track_height, slider->bounds.height};
    }
}

Slider Slider_Create(Rectangle bounds, float min_val, float max_val, float initial_val) {
    Slider slider = {0};
    
    slider.bounds = bounds;
    slider.min_value = min_val;
    slider.max_value = max_val;
    slider.value = Clamp(initial_val, min_val, max_val);
    slider.step_size = AppTheme.slider_props.step_size;
    slider.orientation = SLIDER_HORIZONTAL;
    slider.show_value = AppTheme.slider_props.show_value;
    slider.is_hovered = false;
    slider.is_dragging = false;
    slider.is_disabled = false;
    slider.drag_offset = (Vector2){0, 0};
    
    // Initialize value text
    snprintf(slider.value_text, sizeof(slider.value_text), "%.1f", slider.value);
    
    return slider;
}

Slider Slider_CreateVertical(Rectangle bounds, float min_val, float max_val, float initial_val) {
    Slider slider = Slider_Create(bounds, min_val, max_val, initial_val);
    slider.orientation = SLIDER_VERTICAL;
    return slider;
}

void Slider_Update(Slider* slider) {
    if (slider->is_disabled) {
        slider->is_hovered = false;
        slider->is_dragging = false;
        return;
    }
    
    Vector2 mouse_pos = GetMousePosition();
    Rectangle handle_rect = GetHandleRect(slider);
    
    // Check if mouse is over the slider bounds or handle
    bool mouse_over_slider = CheckCollisionPointRec(mouse_pos, slider->bounds);
    bool mouse_over_handle = CheckCollisionPointRec(mouse_pos, handle_rect);
    
    slider->is_hovered = mouse_over_slider || mouse_over_handle;
    
    // Handle dragging
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse_over_handle) {
        slider->is_dragging = true;
        slider->drag_offset.x = mouse_pos.x - handle_rect.x;
        slider->drag_offset.y = mouse_pos.y - handle_rect.y;
    }
    
    if (slider->is_dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float new_value;
            
            if (slider->orientation == SLIDER_HORIZONTAL) {
                float handle_size = AppTheme.slider_props.handle_size;
                float track_width = slider->bounds.width - handle_size;
                float handle_center_x = mouse_pos.x - slider->drag_offset.x + handle_size * 0.5f;
                float progress = (handle_center_x - slider->bounds.x - handle_size * 0.5f) / track_width;
                progress = Clamp(progress, 0.0f, 1.0f);
                new_value = slider->min_value + progress * (slider->max_value - slider->min_value);
            } else {
                float handle_size = AppTheme.slider_props.handle_size;
                float track_height = slider->bounds.height - handle_size;
                float handle_center_y = mouse_pos.y - slider->drag_offset.y + handle_size * 0.5f;
                float progress = 1.0f - ((handle_center_y - slider->bounds.y - handle_size * 0.5f) / track_height);
                progress = Clamp(progress, 0.0f, 1.0f);
                new_value = slider->min_value + progress * (slider->max_value - slider->min_value);
            }
            
            // Apply step quantization
            new_value = QuantizeToStep(new_value, slider->step_size);
            new_value = Clamp(new_value, slider->min_value, slider->max_value);
            
            if (new_value != slider->value) {
                slider->value = new_value;
                snprintf(slider->value_text, sizeof(slider->value_text), "%.1f", slider->value);
            }
        } else {
            slider->is_dragging = false;
        }
    }
    
    // Handle click on track (jump to position)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse_over_slider && !mouse_over_handle) {
        float new_value;
        
        if (slider->orientation == SLIDER_HORIZONTAL) {
            float track_width = slider->bounds.width - AppTheme.slider_props.handle_size;
            float progress = (mouse_pos.x - slider->bounds.x - AppTheme.slider_props.handle_size * 0.5f) / track_width;
            progress = Clamp(progress, 0.0f, 1.0f);
            new_value = slider->min_value + progress * (slider->max_value - slider->min_value);
        } else {
            float track_height = slider->bounds.height - AppTheme.slider_props.handle_size;
            float progress = 1.0f - ((mouse_pos.y - slider->bounds.y - AppTheme.slider_props.handle_size * 0.5f) / track_height);
            progress = Clamp(progress, 0.0f, 1.0f);
            new_value = slider->min_value + progress * (slider->max_value - slider->min_value);
        }
        
        // Apply step quantization
        new_value = QuantizeToStep(new_value, slider->step_size);
        new_value = Clamp(new_value, slider->min_value, slider->max_value);
        
        if (new_value != slider->value) {
            slider->value = new_value;
            snprintf(slider->value_text, sizeof(slider->value_text), "%.1f", slider->value);
        }
    }
}

void Slider_Draw(Slider* slider) {
    ComponentStyle* style = Theme_GetComponentStyle(COMPONENT_SLIDER);
    SliderProperties* props = &AppTheme.slider_props;
    
    if (!style) return;
    
    // Determine current state for colors
    ComponentState state = STATE_DEFAULT;
    if (slider->is_disabled) {
        state = STATE_DISABLED;
    } else if (slider->is_dragging) {
        state = STATE_PRESSED;
    } else if (slider->is_hovered) {
        state = STATE_HOVER;
    }
    
    ColorPalette* colors = &style->colors[state];
    Rectangle track_rect = GetTrackRect(slider);
    Rectangle handle_rect = GetHandleRect(slider);
    
    // Draw track background
    DrawRectangleRounded(track_rect, props->track_radius / track_rect.height, 8, colors->background);
    
    // Draw track fill (progress)
    Rectangle fill_rect = track_rect;
    float progress = (slider->value - slider->min_value) / (slider->max_value - slider->min_value);
    
    if (slider->orientation == SLIDER_HORIZONTAL) {
        fill_rect.width = fill_rect.width * progress;
    } else {
        float fill_height = fill_rect.height * progress;
        fill_rect.y = fill_rect.y + fill_rect.height - fill_height;
        fill_rect.height = fill_height;
    }
    
    if (progress > 0.0f) {
        DrawRectangleRounded(fill_rect, props->track_radius / fill_rect.height, 8, colors->accent);
    }
    
    // Draw track border
    if (style->border_width.top > 0.0f) {
        DrawRectangleRoundedLines(track_rect, props->track_radius / track_rect.height, 8, 
                                 colors->border);
    }
    
    // Draw handle
    Color handle_color = (state == STATE_HOVER) ? colors->background : 
                        (state == STATE_PRESSED) ? colors->background : 
                        (Color){255, 255, 255, 255}; // Default white handle
    
    DrawCircle((int)(handle_rect.x + handle_rect.width * 0.5f), 
               (int)(handle_rect.y + handle_rect.height * 0.5f), 
               props->handle_radius, handle_color);
    
    // Draw handle border
    if (style->border_width.top > 0.0f) {
        DrawCircleLines((int)(handle_rect.x + handle_rect.width * 0.5f), 
                       (int)(handle_rect.y + handle_rect.height * 0.5f), 
                       props->handle_radius, colors->border);
    }
    
    // Draw value text if enabled
    if (slider->show_value && !slider->is_disabled) {
        Vector2 text_size = MeasureTextEx(GetFontDefault(), slider->value_text, style->font_size, 1.0f);
        Vector2 text_pos;
        
        if (slider->orientation == SLIDER_HORIZONTAL) {
            text_pos.x = slider->bounds.x + slider->bounds.width * 0.5f - text_size.x * 0.5f;
            text_pos.y = slider->bounds.y + slider->bounds.height + 5.0f;
        } else {
            text_pos.x = slider->bounds.x + slider->bounds.width + 5.0f;
            text_pos.y = slider->bounds.y + slider->bounds.height * 0.5f - text_size.y * 0.5f;
        }
        
        DrawTextEx(GetFontDefault(), slider->value_text, text_pos, style->font_size, 1.0f, colors->text);
    }
}

void Slider_SetValue(Slider* slider, float value) {
    float new_value = Clamp(value, slider->min_value, slider->max_value);
    new_value = QuantizeToStep(new_value, slider->step_size);
    
    if (new_value != slider->value) {
        slider->value = new_value;
        snprintf(slider->value_text, sizeof(slider->value_text), "%.1f", slider->value);
    }
}

float Slider_GetValue(Slider* slider) {
    return slider->value;
}

void Slider_SetRange(Slider* slider, float min_val, float max_val) {
    slider->min_value = min_val;
    slider->max_value = max_val;
    
    // Clamp current value to new range
    Slider_SetValue(slider, slider->value);
}

void Slider_SetStepSize(Slider* slider, float step) {
    slider->step_size = step;
    
    // Re-quantize current value to new step size
    Slider_SetValue(slider, slider->value);
}

void Slider_SetShowValue(Slider* slider, bool show) {
    slider->show_value = show;
}

void Slider_SetDisabled(Slider* slider, bool disabled) {
    slider->is_disabled = disabled;
    if (disabled) {
        slider->is_hovered = false;
        slider->is_dragging = false;
    }
}

void Slider_Destroy(Slider* slider) {
    // Sliders don't have dynamic memory allocations to clean up
    // This function exists for API consistency
    if (slider) {
        // Reset to default state
        *slider = (Slider){0};
    }
}