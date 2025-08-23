#ifndef DROPDOWN_H
#define DROPDOWN_H

#include "raylib.h"
#include "button.h"
#include "../vendor/vec.h" // Assumes vec.h is in a vendor folder
#include <stdbool.h>

// Represents a single item in the dropdown list
typedef struct {
    char* text;
    bool is_disabled;
    bool is_separator;
    void* user_data; // Optional user data for each option
} DropdownOption;

// The main Dropdown component struct
typedef struct {
    Button button; // Base button component

    // Core Data
    vec_t(DropdownOption) options; // Dynamic list of all options
    vec_t(int) filtered_options;   // Indices of options that match the current search
    int selected_index;
    bool is_open;

    // Visual Properties
    Rectangle dropdown_rect;     // Cached area for the dropdown list
    float option_height;         // Height of each option row
    float max_dropdown_height;   // Maximum height before scrolling is enabled
    int scroll_offset;           // Current scroll position in pixels
    int hovered_option;          // Index of the currently hovered option

    // Animation Properties
    float open_animation;  // 0.0 (closed) to 1.0 (open) for smooth transitions
    float animation_speed; // Speed of the open/close animation

    // Per-instance Styling (pointers to allow for theme fallback)
    Color* custom_dropdown_bg_color;
    Color* custom_option_hover_color;
    Color* custom_option_text_color;
    Color* custom_separator_color;
    float* custom_dropdown_opacity;

    // Behavior Flags
    bool show_arrow;          // Whether to draw the up/down arrow indicator
    bool close_on_select;     // Close dropdown when an option is selected
    bool allow_deselect;      // Allow deselecting the current selection
    bool search_enabled;      // Enable type-to-search functionality

    // Search State
    char search_buffer[64];
    float search_timer; // Timer to clear the search buffer after inactivity

} Dropdown;

// --- Function Prototypes ---

// Core Functions
Dropdown Dropdown_Create(Rectangle rect, const char* button_text, ButtonVariant variant);
void Dropdown_Update(Dropdown* dropdown);
void Dropdown_Draw(const Dropdown* dropdown);
void Dropdown_Destroy(Dropdown* dropdown);

// Option Management
int Dropdown_AddOption(Dropdown* dropdown, const char* text);
int Dropdown_AddSeparator(Dropdown* dropdown);
void Dropdown_RemoveOption(Dropdown* dropdown, int index);
void Dropdown_ClearOptions(Dropdown* dropdown);
void Dropdown_SetOptionDisabled(Dropdown* dropdown, int index, bool disabled);
void Dropdown_SetOptionUserData(Dropdown* dropdown, int index, void* user_data);

// Selection Management
void Dropdown_SetSelected(Dropdown* dropdown, int index);
int Dropdown_GetSelected(const Dropdown* dropdown);
const char* Dropdown_GetSelectedText(const Dropdown* dropdown);
void* Dropdown_GetSelectedUserData(const Dropdown* dropdown);
bool Dropdown_HasSelection(const Dropdown* dropdown);

// State Management
void Dropdown_Open(Dropdown* dropdown);
void Dropdown_Close(Dropdown* dropdown);
void Dropdown_Toggle(Dropdown* dropdown);
bool Dropdown_IsOpen(const Dropdown* dropdown);

// Visual Customization
void Dropdown_SetOptionHeight(Dropdown* dropdown, float height);
void Dropdown_SetMaxHeight(Dropdown* dropdown, float max_height);
void Dropdown_SetAnimationSpeed(Dropdown* dropdown, float speed);
void Dropdown_SetShowArrow(Dropdown* dropdown, bool show_arrow);
void Dropdown_SetDropdownBackgroundColor(Dropdown* dropdown, Color color);
void Dropdown_SetOptionHoverColor(Dropdown* dropdown, Color color);
void Dropdown_SetOptionTextColor(Dropdown* dropdown, Color color);
void Dropdown_SetSeparatorColor(Dropdown* dropdown, Color color);
void Dropdown_SetDropdownOpacity(Dropdown* dropdown, float opacity);

// Behavior Configuration
void Dropdown_SetCloseOnSelect(Dropdown* dropdown, bool close_on_select);
void Dropdown_SetAllowDeselect(Dropdown* dropdown, bool allow_deselect);
void Dropdown_SetSearchEnabled(Dropdown* dropdown, bool search_enabled);

// Utility Functions
int Dropdown_FindOptionByText(const Dropdown* dropdown, const char* text);
void Dropdown_ScrollToOption(Dropdown* dropdown, int index);
Rectangle Dropdown_GetOptionRect(const Dropdown* dropdown, int option_index);

// Reset Functions
void Dropdown_ResetCustomStyles(Dropdown* dropdown);

#endif // DROPDOWN_H