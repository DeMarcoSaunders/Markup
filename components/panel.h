#ifndef PANEL_H
#define PANEL_H

#include "raylib.h"
#include "vendor/vec.h"
#include "theme.h"
#include "button.h"

// Forward declarations
typedef struct Panel Panel;
// Remove Button forward declaration since it's defined in button.h
// typedef struct Slider Slider;
// Remove Slider and Sidebar forward declarations - they may not be implemented yet
// typedef struct Sidebar Sidebar;

// Flexbox enums (reusing from sidebar design)
typedef enum {
    FLEX_DIRECTION_COLUMN,
    FLEX_DIRECTION_ROW
} FlexDirection;

typedef enum {
    JUSTIFY_START,
    JUSTIFY_CENTER,
    JUSTIFY_END,
    JUSTIFY_SPACE_BETWEEN,
    JUSTIFY_SPACE_AROUND,
    JUSTIFY_SPACE_EVENLY
} JustifyContent;

typedef enum {
    ALIGN_START,
    ALIGN_CENTER,
    ALIGN_END,
    ALIGN_STRETCH
} AlignItems;

typedef enum {
    PANEL_CHILD_BUTTON,
    PANEL_CHILD_PANEL,
    PANEL_CHILD_SLIDER,
    PANEL_CHILD_SIDEBAR,
    PANEL_CHILD_CUSTOM
} PanelChildType;

typedef struct {
    PanelChildType type;
    void* component;  // Pointer to actual component (Button*, Panel*, Slider*, Sidebar*, etc.)
    
    // Flexbox properties for this child
    int flex_grow;    // How much this child should grow (0 = no grow)
    int flex_shrink;  // How much this child should shrink (1 = can shrink)
    float flex_basis; // Initial size before free space distribution
    
    // Individual alignment override
    AlignItems align_self; // Override parent's align_items for this child
    
    // Margin for spacing
    EdgeValues margin;
    
    // Visibility and interaction
    bool is_visible;
    bool is_interactive; // Whether this child should handle input
    
    // Layout results (calculated during layout)
    Rectangle calculated_rect;
    bool needs_layout_update;
} PanelChild;

// Dynamic array type for panel children using vec.h
typedef vec_t(PanelChild) PanelChildren;

typedef struct Panel {
    Rectangle rect;
    bool is_visible;
    
    // Flexbox-like layout system
    PanelChildren children;  // Dynamic array of child components
    FlexDirection flex_direction;
    JustifyContent justify_content;
    AlignItems align_items;
    float gap; // Space between children
    
    // Layout cache
    Rectangle content_area; // Available area for children after padding
    bool needs_layout_recalculation;
    
    // Layout and positioning
    int z_index;
    
    // Content overflow handling
    bool clip_content;  // Whether to clip child content to panel bounds
} Panel;

// Core Functions
Panel Panel_Create(Rectangle rect);
void Panel_Update(Panel* panel);
void Panel_Draw(const Panel* panel);
void Panel_Destroy(Panel* panel);

// Child Component Management
int Panel_AddButton(Panel* panel, Button* button, int flex_grow, float flex_basis);
int Panel_AddPanel(Panel* panel, Panel* child_panel, int flex_grow, float flex_basis);
int Panel_AddSlider(Panel* panel, void* slider, int flex_grow, float flex_basis);
int Panel_AddSidebar(Panel* panel, void* sidebar, int flex_grow, float flex_basis);
int Panel_AddDropdown(Panel* panel, void* dropdown, int flex_grow, float flex_basis);
int Panel_AddTextInput(Panel* panel, void* text_input, int flex_grow, float flex_basis);
int Panel_AddTextArea(Panel* panel, void* text_area, int flex_grow, float flex_basis);
int Panel_AddCustom(Panel* panel, void* component, int flex_grow, float flex_basis);
void Panel_RemoveChild(Panel* panel, int index);
void Panel_ClearChildren(Panel* panel);
int Panel_GetChildCount(const Panel* panel);
PanelChild* Panel_GetChild(Panel* panel, int index);

// Flexbox Layout Functions
void Panel_SetFlexDirection(Panel* panel, FlexDirection direction);
void Panel_SetJustifyContent(Panel* panel, JustifyContent justify);
void Panel_SetAlignItems(Panel* panel, AlignItems align);
void Panel_SetGap(Panel* panel, float gap);
void Panel_SetChildFlexGrow(Panel* panel, int child_index, int flex_grow);
void Panel_SetChildFlexShrink(Panel* panel, int child_index, int flex_shrink);
void Panel_SetChildFlexBasis(Panel* panel, int child_index, float flex_basis);
void Panel_SetChildAlignSelf(Panel* panel, int child_index, AlignItems align_self);
void Panel_SetChildMargin(Panel* panel, int child_index, EdgeValues margin);

// Layout Calculation
void Panel_RecalculateLayout(Panel* panel);
Rectangle Panel_GetChildRect(const Panel* panel, int child_index);
Rectangle Panel_GetContentArea(const Panel* panel);

// Visual State Functions
void Panel_SetVisible(Panel* panel, bool is_visible);
void Panel_SetClipContent(Panel* panel, bool clip_content);
void Panel_SetZIndex(Panel* panel, int z_index);

#endif // PANEL_H