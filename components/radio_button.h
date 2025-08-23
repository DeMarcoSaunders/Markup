#ifndef RADIO_BUTTON_H
#define RADIO_BUTTON_H

#include "raylib.h"
#include "theme.h"
#include <stdbool.h>

// Forward declaration for radio group
typedef struct RadioGroup RadioGroup;

typedef struct {
    Rectangle bounds;
    char* label;
    int value;
    bool is_selected;
    
    // State
    bool is_hovered;
    bool is_pressed;
    bool is_disabled;
    bool is_visible;
    
    // Animation
    float select_animation;
    float target_animation;
    
    // Group association
    RadioGroup* group;
    
    // Visual customization
    bool show_label;
    bool show_shadow;
    bool show_border;
    
    // Custom styling (optional overrides)
    Color* custom_circle_color;
    Color* custom_dot_color;
    Color* custom_border_color;
    Color* custom_label_color;
    EdgeValues* custom_padding;
    int* custom_font_size;
    float* custom_circle_size;
    float* custom_dot_size;
    
    // Callbacks
    void (*on_selected)(int value, void* user_data);
    void* user_data;
} RadioButton;

// Radio group for managing multiple radio buttons
struct RadioGroup {
    RadioButton** buttons;
    int button_count;
    int capacity;
    int selected_value;
    
    // Callbacks
    void (*on_selection_changed)(int old_value, int new_value, void* user_data);
    void* user_data;
};

// RadioButton functions
RadioButton RadioButton_Create(Rectangle bounds, const char* label, int value);
void RadioButton_Destroy(RadioButton* radio);

// Update and drawing
void RadioButton_Update(RadioButton* radio);
void RadioButton_Draw(RadioButton* radio);

// State management
void RadioButton_SetSelected(RadioButton* radio, bool selected);
bool RadioButton_IsSelected(RadioButton* radio);
void RadioButton_SetDisabled(RadioButton* radio, bool disabled);
void RadioButton_SetVisible(RadioButton* radio, bool visible);
int RadioButton_GetValue(RadioButton* radio);

// Label management
void RadioButton_SetLabel(RadioButton* radio, const char* label);
const char* RadioButton_GetLabel(RadioButton* radio);

// Visual customization
void RadioButton_SetShowLabel(RadioButton* radio, bool show);
void RadioButton_SetShowShadow(RadioButton* radio, bool show);
void RadioButton_SetShowBorder(RadioButton* radio, bool show);

// Styling
void RadioButton_SetCircleColor(RadioButton* radio, Color color);
void RadioButton_SetDotColor(RadioButton* radio, Color color);
void RadioButton_SetBorderColor(RadioButton* radio, Color color);
void RadioButton_SetLabelColor(RadioButton* radio, Color color);
void RadioButton_SetPadding(RadioButton* radio, float top, float right, float bottom, float left);
void RadioButton_SetFontSize(RadioButton* radio, int font_size);
void RadioButton_SetCircleSize(RadioButton* radio, float size);
void RadioButton_SetDotSize(RadioButton* radio, float size);

// Callbacks
void RadioButton_SetOnSelected(RadioButton* radio, void (*callback)(int, void*), void* user_data);

// Utility functions
void RadioButton_ResetCustomStyles(RadioButton* radio);
Rectangle RadioButton_GetCircleBounds(RadioButton* radio);
Rectangle RadioButton_GetLabelBounds(RadioButton* radio);

// RadioGroup functions
RadioGroup* RadioGroup_Create(void);
void RadioGroup_Destroy(RadioGroup* group);

// Group management
void RadioGroup_AddButton(RadioGroup* group, RadioButton* radio);
void RadioGroup_RemoveButton(RadioGroup* group, RadioButton* radio);
void RadioGroup_ClearButtons(RadioGroup* group);

// Selection management
void RadioGroup_SetSelected(RadioGroup* group, int value);
int RadioGroup_GetSelected(RadioGroup* group);
RadioButton* RadioGroup_GetSelectedButton(RadioGroup* group);

// Group operations
void RadioGroup_UpdateAll(RadioGroup* group);
void RadioGroup_DrawAll(RadioGroup* group);
void RadioGroup_SetAllDisabled(RadioGroup* group, bool disabled);
void RadioGroup_SetAllVisible(RadioGroup* group, bool visible);

// Callbacks
void RadioGroup_SetOnSelectionChanged(RadioGroup* group, 
                                     void (*callback)(int, int, void*), 
                                     void* user_data);

#endif // RADIO_BUTTON_H