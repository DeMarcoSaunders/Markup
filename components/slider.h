#ifndef SLIDER_H
#define SLIDER_H

#include "raylib.h"
#include <stdbool.h>

typedef enum {
    SLIDER_HORIZONTAL,
    SLIDER_VERTICAL
} SliderOrientation;

typedef struct {
    Rectangle bounds;
    float value;
    float min_value;
    float max_value;
    float step_size;
    SliderOrientation orientation;
    bool show_value;
    bool is_hovered;
    bool is_dragging;
    bool is_disabled;
    
    // Internal state for interaction
    Vector2 drag_offset;
    char value_text[32];
} Slider;

// Function declarations
Slider Slider_Create(Rectangle bounds, float min_val, float max_val, float initial_val);
Slider Slider_CreateVertical(Rectangle bounds, float min_val, float max_val, float initial_val);
void Slider_Update(Slider* slider);
void Slider_Draw(Slider* slider);
void Slider_Destroy(Slider* slider);
void Slider_SetValue(Slider* slider, float value);
float Slider_GetValue(Slider* slider);
void Slider_SetRange(Slider* slider, float min_val, float max_val);
void Slider_SetStepSize(Slider* slider, float step);
void Slider_SetShowValue(Slider* slider, bool show);
void Slider_SetDisabled(Slider* slider, bool disabled);

#endif // SLIDER_H