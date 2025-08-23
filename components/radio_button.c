#include "radio_button.h"
#include "shadow.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper function to get effective style values
static ComponentStyle* GetEffectiveStyle(RadioButton* radio) {
    return Theme_GetComponentStyle(COMPONENT_RADIO_BUTTON);
}

static ComponentState GetCurrentState(RadioButton* radio) {
    if (radio->is_disabled) return STATE_DISABLED;
    if (radio->is_selected) return STATE_ACTIVE;
    if (radio->is_pressed) return STATE_PRESSED;
    if (radio->is_hovered) return STATE_HOVER;
    return STATE_DEFAULT;
}

// Helper function for smooth animation easing
static float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

RadioButton RadioButton_Create(Rectangle bounds, const char* label, int value) {
    RadioButton radio = {0};
    
    radio.bounds = bounds;
    radio.value = value;
    radio.is_selected = false;
    
    // Set label
    if (label) {
        size_t label_len = strlen(label) + 1;
        radio.label = (char*)malloc(label_len);
        strcpy(radio.label, label);
    }
    
    // Initialize state
    radio.is_hovered = false;
    radio.is_pressed = false;
    radio.is_disabled = false;
    radio.is_visible = true;
    
    // Initialize animation
    radio.select_animation = 0.0f;
    radio.target_animation = 0.0f;
    
    // Initialize group
    radio.group = NULL;
    
    // Initialize visual settings
    radio.show_label = true;
    radio.show_shadow = false;
    radio.show_border = true;
    
    // Initialize custom styling pointers to NULL
    radio.custom_circle_color = NULL;
    radio.custom_dot_color = NULL;
    radio.custom_border_color = NULL;
    radio.custom_label_color = NULL;
    radio.custom_padding = NULL;
    radio.custom_font_size = NULL;
    radio.custom_circle_size = NULL;
    radio.custom_dot_size = NULL;
    
    // Initialize callbacks
    radio.on_selected = NULL;
    radio.user_data = NULL;
    
    return radio;
}

void RadioButton_Destroy(RadioButton* radio) {
    if (!radio) return;
    
    // Remove from group if associated
    if (radio->group) {
        RadioGroup_RemoveButton(radio->group, radio);
    }
    
    if (radio->label) {
        free(radio->label);
        radio->label = NULL;
    }
    
    // Free custom styling
    if (radio->custom_circle_color) { free(radio->custom_circle_color); radio->custom_circle_color = NULL; }
    if (radio->custom_dot_color) { free(radio->custom_dot_color); radio->custom_dot_color = NULL; }
    if (radio->custom_border_color) { free(radio->custom_border_color); radio->custom_border_color = NULL; }
    if (radio->custom_label_color) { free(radio->custom_label_color); radio->custom_label_color = NULL; }
    if (radio->custom_padding) { free(radio->custom_padding); radio->custom_padding = NULL; }
    if (radio->custom_font_size) { free(radio->custom_font_size); radio->custom_font_size = NULL; }
    if (radio->custom_circle_size) { free(radio->custom_circle_size); radio->custom_circle_size = NULL; }
    if (radio->custom_dot_size) { free(radio->custom_dot_size); radio->custom_dot_size = NULL; }
}

void RadioButton_Update(RadioButton* radio) {
    if (!radio || !radio->is_visible || radio->is_disabled) {
        return;
    }
    
    Vector2 mouse_pos = GetMousePosition();
    bool mouse_over = CheckCollisionPointRec(mouse_pos, radio->bounds);
    
    radio->is_hovered = mouse_over;
    
    // Handle mouse interaction
    if (mouse_over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        radio->is_pressed = true;
    }
    
    if (radio->is_pressed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (mouse_over) {
            // Select this radio button
            if (radio->group) {
                RadioGroup_SetSelected(radio->group, radio->value);
            } else {
                RadioButton_SetSelected(radio, true);
            }
        }
        radio->is_pressed = false;
    }
    
    // Update animation
    float animation_speed = AppTheme.radio_button_props.animation_speed * GetFrameTime();
    if (radio->select_animation < radio->target_animation) {
        radio->select_animation = fminf(radio->target_animation, 
                                       radio->select_animation + animation_speed);
    } else if (radio->select_animation > radio->target_animation) {
        radio->select_animation = fmaxf(radio->target_animation, 
                                       radio->select_animation - animation_speed);
    }
}

void RadioButton_Draw(RadioButton* radio) {
    if (!radio || !radio->is_visible) return;
    
    ComponentStyle* style = GetEffectiveStyle(radio);
    ComponentState state = GetCurrentState(radio);
    ColorPalette* colors = &style->colors[state];
    
    // Get effective values
    EdgeValues padding = radio->custom_padding ? *radio->custom_padding : style->padding;
    int font_size = radio->custom_font_size ? *radio->custom_font_size : style->font_size;
    float circle_size = radio->custom_circle_size ? *radio->custom_circle_size : AppTheme.radio_button_props.circle_size;
    float dot_size = radio->custom_dot_size ? *radio->custom_dot_size : AppTheme.radio_button_props.dot_size;
    
    Color circle_color = radio->custom_circle_color ? *radio->custom_circle_color : colors->background;
    Color dot_color = radio->custom_dot_color ? *radio->custom_dot_color : colors->accent;
    Color border_color = radio->custom_border_color ? *radio->custom_border_color : colors->border;
    Color label_color = radio->custom_label_color ? *radio->custom_label_color : colors->text;
    
    // Calculate radio button circle center
    Vector2 circle_center = {
        radio->bounds.x + padding.left + circle_size * 0.5f,
        radio->bounds.y + radio->bounds.height * 0.5f
    };
    
    float circle_radius = circle_size * 0.5f;
    
    // Draw shadow
    if (radio->show_shadow) {
        ShadowConfig shadow_config = Shadow_FromTheme(style, state);
        Shadow_DrawCircle(circle_center, circle_radius, &shadow_config);
    }
    
    // Animate colors for selected state
    if (radio->is_selected && radio->select_animation > 0.0f) {
        float anim_progress = EaseOutCubic(radio->select_animation);
        
        // Interpolate border color to selected state
        ColorPalette* active_colors = &style->colors[STATE_ACTIVE];
        border_color = (Color){
            (unsigned char)(border_color.r + (active_colors->border.r - border_color.r) * anim_progress),
            (unsigned char)(border_color.g + (active_colors->border.g - border_color.g) * anim_progress),
            (unsigned char)(border_color.b + (active_colors->border.b - border_color.b) * anim_progress),
            border_color.a
        };
    }
    
    // Draw radio button circle
    DrawCircle((int)circle_center.x, (int)circle_center.y, circle_radius, circle_color);
    
    // Draw border
    if (radio->show_border) {
        DrawCircleLines((int)circle_center.x, (int)circle_center.y, circle_radius, border_color);
    }
    
    // Draw dot (when selected)
    if (radio->select_animation > 0.0f) {
        float anim_progress = EaseOutCubic(radio->select_animation);
        float animated_dot_radius = (dot_size * 0.5f) * anim_progress;
        
        Color animated_dot_color = dot_color;
        animated_dot_color.a = (unsigned char)(dot_color.a * anim_progress);
        
        DrawCircle((int)circle_center.x, (int)circle_center.y, animated_dot_radius, animated_dot_color);
    }
    
    // Draw label
    if (radio->show_label && radio->label) {
        Vector2 label_pos = {
            circle_center.x + circle_radius + AppTheme.radio_button_props.label_spacing,
            radio->bounds.y + (radio->bounds.height - font_size) * 0.5f
        };
        
        DrawTextEx(GetFontDefault(), radio->label, label_pos, font_size, 1.0f, label_color);
    }
}

// State management functions
void RadioButton_SetSelected(RadioButton* radio, bool selected) {
    if (!radio || radio->is_disabled) return;
    
    bool was_selected = radio->is_selected;
    radio->is_selected = selected;
    radio->target_animation = selected ? 1.0f : 0.0f;
    
    if (was_selected != selected && selected && radio->on_selected) {
        radio->on_selected(radio->value, radio->user_data);
    }
}

bool RadioButton_IsSelected(RadioButton* radio) {
    return radio ? radio->is_selected : false;
}

void RadioButton_SetDisabled(RadioButton* radio, bool disabled) {
    if (radio) {
        radio->is_disabled = disabled;
        if (disabled) {
            radio->is_hovered = false;
            radio->is_pressed = false;
        }
    }
}

void RadioButton_SetVisible(RadioButton* radio, bool visible) {
    if (radio) radio->is_visible = visible;
}

int RadioButton_GetValue(RadioButton* radio) {
    return radio ? radio->value : -1;
}

// Label management functions
void RadioButton_SetLabel(RadioButton* radio, const char* label) {
    if (!radio) return;
    
    if (radio->label) {
        free(radio->label);
        radio->label = NULL;
    }
    
    if (label) {
        size_t label_len = strlen(label) + 1;
        radio->label = (char*)malloc(label_len);
        strcpy(radio->label, label);
    }
}

const char* RadioButton_GetLabel(RadioButton* radio) {
    return radio ? radio->label : NULL;
}

// Visual customization functions
void RadioButton_SetShowLabel(RadioButton* radio, bool show) {
    if (radio) radio->show_label = show;
}

void RadioButton_SetShowShadow(RadioButton* radio, bool show) {
    if (radio) radio->show_shadow = show;
}

void RadioButton_SetShowBorder(RadioButton* radio, bool show) {
    if (radio) radio->show_border = show;
}

// Styling functions (similar to checkbox, but for circle and dot)
void RadioButton_SetCircleColor(RadioButton* radio, Color color) {
    if (!radio) return;
    
    if (!radio->custom_circle_color) {
        radio->custom_circle_color = (Color*)malloc(sizeof(Color));
    }
    
    if (radio->custom_circle_color) {
        *radio->custom_circle_color = color;
    }
}

void RadioButton_SetDotColor(RadioButton* radio, Color color) {
    if (!radio) return;
    
    if (!radio->custom_dot_color) {
        radio->custom_dot_color = (Color*)malloc(sizeof(Color));
    }
    
    if (radio->custom_dot_color) {
        *radio->custom_dot_color = color;
    }
}

// Callback functions
void RadioButton_SetOnSelected(RadioButton* radio, void (*callback)(int, void*), void* user_data) {
    if (!radio) return;
    
    radio->on_selected = callback;
    radio->user_data = user_data;
}

// Utility functions
Rectangle RadioButton_GetCircleBounds(RadioButton* radio) {
    if (!radio) return (Rectangle){0, 0, 0, 0};
    
    ComponentStyle* style = GetEffectiveStyle(radio);
    EdgeValues padding = radio->custom_padding ? *radio->custom_padding : style->padding;
    float circle_size = radio->custom_circle_size ? *radio->custom_circle_size : AppTheme.radio_button_props.circle_size;
    
    return (Rectangle){
        radio->bounds.x + padding.left,
        radio->bounds.y + (radio->bounds.height - circle_size) * 0.5f,
        circle_size,
        circle_size
    };
}

// RadioGroup implementation
RadioGroup* RadioGroup_Create(void) {
    RadioGroup* group = (RadioGroup*)malloc(sizeof(RadioGroup));
    if (!group) return NULL;
    
    group->buttons = NULL;
    group->button_count = 0;
    group->capacity = 0;
    group->selected_value = -1;
    group->on_selection_changed = NULL;
    group->user_data = NULL;
    
    return group;
}

void RadioGroup_Destroy(RadioGroup* group) {
    if (!group) return;
    
    // Remove group association from all buttons
    for (int i = 0; i < group->button_count; i++) {
        if (group->buttons[i]) {
            group->buttons[i]->group = NULL;
        }
    }
    
    if (group->buttons) {
        free(group->buttons);
    }
    
    free(group);
}

void RadioGroup_AddButton(RadioGroup* group, RadioButton* radio) {
    if (!group || !radio) return;
    
    // Remove from previous group if any
    if (radio->group && radio->group != group) {
        RadioGroup_RemoveButton(radio->group, radio);
    }
    
    // Expand array if needed
    if (group->button_count >= group->capacity) {
        int new_capacity = group->capacity == 0 ? 4 : group->capacity * 2;
        RadioButton** new_buttons = (RadioButton**)realloc(group->buttons, 
                                                           new_capacity * sizeof(RadioButton*));
        if (!new_buttons) return;
        
        group->buttons = new_buttons;
        group->capacity = new_capacity;
    }
    
    // Add button to group
    group->buttons[group->button_count] = radio;
    group->button_count++;
    radio->group = group;
    
    // If this is the first button or it's already selected, make it the selected one
    if (group->button_count == 1 || radio->is_selected) {
        RadioGroup_SetSelected(group, radio->value);
    }
}

void RadioGroup_RemoveButton(RadioGroup* group, RadioButton* radio) {
    if (!group || !radio) return;
    
    // Find and remove button
    for (int i = 0; i < group->button_count; i++) {
        if (group->buttons[i] == radio) {
            // Shift remaining buttons
            for (int j = i; j < group->button_count - 1; j++) {
                group->buttons[j] = group->buttons[j + 1];
            }
            group->button_count--;
            radio->group = NULL;
            
            // If this was the selected button, clear selection
            if (radio->value == group->selected_value) {
                group->selected_value = -1;
            }
            break;
        }
    }
}

void RadioGroup_SetSelected(RadioGroup* group, int value) {
    if (!group) return;
    
    int old_value = group->selected_value;
    group->selected_value = value;
    
    // Update all buttons in the group
    for (int i = 0; i < group->button_count; i++) {
        RadioButton* radio = group->buttons[i];
        if (radio) {
            bool should_be_selected = (radio->value == value);
            if (radio->is_selected != should_be_selected) {
                RadioButton_SetSelected(radio, should_be_selected);
            }
        }
    }
    
    // Trigger group callback
    if (old_value != value && group->on_selection_changed) {
        group->on_selection_changed(old_value, value, group->user_data);
    }
}

int RadioGroup_GetSelected(RadioGroup* group) {
    return group ? group->selected_value : -1;
}

RadioButton* RadioGroup_GetSelectedButton(RadioGroup* group) {
    if (!group) return NULL;
    
    for (int i = 0; i < group->button_count; i++) {
        if (group->buttons[i] && group->buttons[i]->value == group->selected_value) {
            return group->buttons[i];
        }
    }
    
    return NULL;
}

void RadioGroup_UpdateAll(RadioGroup* group) {
    if (!group) return;
    
    for (int i = 0; i < group->button_count; i++) {
        if (group->buttons[i]) {
            RadioButton_Update(group->buttons[i]);
        }
    }
}

void RadioGroup_DrawAll(RadioGroup* group) {
    if (!group) return;
    
    for (int i = 0; i < group->button_count; i++) {
        if (group->buttons[i]) {
            RadioButton_Draw(group->buttons[i]);
        }
    }
}

void RadioGroup_SetAllDisabled(RadioGroup* group, bool disabled) {
    if (!group) return;
    
    for (int i = 0; i < group->button_count; i++) {
        if (group->buttons[i]) {
            RadioButton_SetDisabled(group->buttons[i], disabled);
        }
    }
}

void RadioGroup_SetAllVisible(RadioGroup* group, bool visible) {
    if (!group) return;
    
    for (int i = 0; i < group->button_count; i++) {
        if (group->buttons[i]) {
            RadioButton_SetVisible(group->buttons[i], visible);
        }
    }
}

void RadioGroup_SetOnSelectionChanged(RadioGroup* group, 
                                     void (*callback)(int, int, void*), 
                                     void* user_data) {
    if (!group) return;
    
    group->on_selection_changed = callback;
    group->user_data = user_data;
}