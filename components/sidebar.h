#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "raylib.h"
#include "vendor/vec.h"
#include "theme.h"
#include "components/panel.h"
#include <stdbool.h>

// Sidebar position enum
typedef enum {
    SIDEBAR_LEFT,
    SIDEBAR_RIGHT,
    SIDEBAR_TOP,
    SIDEBAR_BOTTOM
} SidebarPosition;

// Sidebar state enum
typedef enum {
    SIDEBAR_STATE_COLLAPSED,
    SIDEBAR_STATE_EXPANDED,
    SIDEBAR_STATE_ANIMATING
} SidebarState;

// Component types for sidebar children
typedef enum {
    SIDEBAR_CHILD_BUTTON,
    SIDEBAR_CHILD_PANEL,
    SIDEBAR_CHILD_SLIDER,
    SIDEBAR_CHILD_CUSTOM
} SidebarChildType;

// Sidebar child structure
typedef struct {
    SidebarChildType type;
    void* component;  // Pointer to actual component
    bool is_visible;
    bool is_interactive;
    int flex_grow;
    int flex_shrink;
    float flex_basis;
    EdgeValues margin;
} SidebarChild;

// Main sidebar structure
typedef struct {
    Rectangle bounds;
    SidebarPosition position;
    SidebarState state;
    
    // Layout properties
    FlexDirection flex_direction;
    JustifyContent justify_content;
    AlignItems align_items;
    float gap;
    
    // Animation
    float animation_progress;  // 0.0 = collapsed, 1.0 = expanded
    float animation_speed;
    bool is_animating;
    
    // Children
    vec_t(SidebarChild) children;
    
    // Visual properties
    bool show_border;
    bool show_shadow;
    bool is_visible;
    bool is_interactive;
    int z_index;
    
    // Custom styling
    Color* custom_background_color;
    Color* custom_border_color;
    EdgeValues* custom_padding;
    EdgeValues* custom_margin;
    CornerRadius* custom_border_radius;
    
    // Callbacks
    void (*on_state_changed)(SidebarState new_state, void* user_data);
    void* user_data;
} Sidebar;

// Creation and destruction
Sidebar Sidebar_Create(Rectangle bounds, SidebarPosition position);
void Sidebar_Destroy(Sidebar* sidebar);

// Update and drawing
void Sidebar_Update(Sidebar* sidebar);
void Sidebar_Draw(const Sidebar* sidebar);

// State management
void Sidebar_SetState(Sidebar* sidebar, SidebarState state);
SidebarState Sidebar_GetState(const Sidebar* sidebar);
void Sidebar_Toggle(Sidebar* sidebar);
void Sidebar_Expand(Sidebar* sidebar);
void Sidebar_Collapse(Sidebar* sidebar);

// Layout management
void Sidebar_SetFlexDirection(Sidebar* sidebar, FlexDirection direction);
void Sidebar_SetJustifyContent(Sidebar* sidebar, JustifyContent justify);
void Sidebar_SetAlignItems(Sidebar* sidebar, AlignItems align);
void Sidebar_SetGap(Sidebar* sidebar, float gap);

// Child management
int Sidebar_AddButton(Sidebar* sidebar, void* button);
int Sidebar_AddPanel(Sidebar* sidebar, void* panel);
int Sidebar_AddSlider(Sidebar* sidebar, void* slider);
int Sidebar_AddCustom(Sidebar* sidebar, void* component, SidebarChildType type);
void Sidebar_RemoveChild(Sidebar* sidebar, int index);
void Sidebar_ClearChildren(Sidebar* sidebar);

// Child layout properties
void Sidebar_SetChildFlexGrow(Sidebar* sidebar, int child_index, int flex_grow);
void Sidebar_SetChildFlexShrink(Sidebar* sidebar, int child_index, int flex_shrink);
void Sidebar_SetChildFlexBasis(Sidebar* sidebar, int child_index, float flex_basis);
void Sidebar_SetChildMargin(Sidebar* sidebar, int child_index, EdgeValues margin);
void Sidebar_SetChildVisible(Sidebar* sidebar, int child_index, bool visible);
void Sidebar_SetChildInteractive(Sidebar* sidebar, int child_index, bool interactive);

// Visual customization
void Sidebar_SetBorder(Sidebar* sidebar, bool show_border);
void Sidebar_SetShadow(Sidebar* sidebar, bool show_shadow);
void Sidebar_SetVisible(Sidebar* sidebar, bool visible);
void Sidebar_SetInteractive(Sidebar* sidebar, bool interactive);
void Sidebar_SetZIndex(Sidebar* sidebar, int z_index);

// Styling functions
void Sidebar_SetBackgroundColor(Sidebar* sidebar, Color color);
void Sidebar_SetBorderColor(Sidebar* sidebar, Color color);
void Sidebar_SetPadding(Sidebar* sidebar, float top, float right, float bottom, float left);
void Sidebar_SetMargin(Sidebar* sidebar, float top, float right, float bottom, float left);
void Sidebar_SetBorderRadius(Sidebar* sidebar, float top_left, float top_right, float bottom_right, float bottom_left);

// Animation
void Sidebar_SetAnimationSpeed(Sidebar* sidebar, float speed);
float Sidebar_GetAnimationProgress(const Sidebar* sidebar);
bool Sidebar_IsAnimating(const Sidebar* sidebar);

// Callbacks
void Sidebar_SetOnStateChanged(Sidebar* sidebar, void (*callback)(SidebarState, void*), void* user_data);

// Utility functions
Rectangle Sidebar_GetBounds(const Sidebar* sidebar);
bool Sidebar_IsExpanded(const Sidebar* sidebar);
bool Sidebar_IsCollapsed(const Sidebar* sidebar);
int Sidebar_GetChildCount(const Sidebar* sidebar);

// Reset functions
void Sidebar_ResetCustomStyles(Sidebar* sidebar);

#endif // SIDEBAR_H 