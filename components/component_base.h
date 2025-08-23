#ifndef COMPONENT_BASE_H
#define COMPONENT_BASE_H

#include "raylib.h"
#include "error_handling.h"
#include <stdbool.h>

// Component lifecycle states
typedef enum {
    COMPONENT_STATE_UNINITIALIZED,
    COMPONENT_STATE_INITIALIZED,
    COMPONENT_STATE_ACTIVE,
    COMPONENT_STATE_INACTIVE,
    COMPONENT_STATE_DESTROYED
} ComponentState;

// Component type identification
typedef enum {
    COMPONENT_TYPE_BUTTON,
    COMPONENT_TYPE_PANEL,
    COMPONENT_TYPE_SIDEBAR,
    COMPONENT_TYPE_SLIDER,
    COMPONENT_TYPE_DROPDOWN,
    COMPONENT_TYPE_TEXT_INPUT,
    COMPONENT_TYPE_TEXT_AREA,
    COMPONENT_TYPE_CHECKBOX,
    COMPONENT_TYPE_RADIO_BUTTON,
    COMPONENT_TYPE_MODAL,
    COMPONENT_TYPE_IMAGEBOX,
    COMPONENT_TYPE_SEPARATOR,
    COMPONENT_TYPE_BACKDROP,
    COMPONENT_TYPE_BLUR_EFFECT,
    COMPONENT_TYPE_CUSTOM
} ComponentType;

// Base component structure that all components should include
typedef struct {
    ComponentType type;
    ComponentState state;
    Rectangle rect;
    bool is_visible;
    bool is_interactive;
    bool is_focused;
    bool is_hovered;
    bool is_pressed;
    bool is_disabled;
    
    // Unique identifier for component tracking
    unsigned int id;
    
    // Parent-child relationships
    struct ComponentBase* parent;
    struct ComponentBase** children;
    int child_count;
    int child_capacity;
    
    // Event callbacks (optional)
    void (*on_click)(struct ComponentBase* component, Vector2 position);
    void (*on_hover)(struct ComponentBase* component, bool is_hovered);
    void (*on_focus)(struct ComponentBase* component, bool is_focused);
    void (*on_key_press)(struct ComponentBase* component, int key);
    void (*on_text_input)(struct ComponentBase* component, const char* text);
    
    // User data pointer for custom data
    void* user_data;
    
    // Layout and positioning
    int z_index;
    Vector2 transform_offset;
    float rotation;
    Vector2 scale;
    
    // Validation flags
    bool needs_layout_update;
    bool needs_redraw;
    bool needs_validation;
} ComponentBase;

// Component lifecycle functions
bool ComponentBase_Init(ComponentBase* component, ComponentType type, Rectangle rect);
void ComponentBase_Update(ComponentBase* component);
void ComponentBase_Draw(ComponentBase* component);
void ComponentBase_Destroy(ComponentBase* component);
bool ComponentBase_Validate(ComponentBase* component);

// Parent-child relationship management
bool ComponentBase_AddChild(ComponentBase* parent, ComponentBase* child);
bool ComponentBase_RemoveChild(ComponentBase* parent, ComponentBase* child);
bool ComponentBase_SetParent(ComponentBase* component, ComponentBase* parent);
ComponentBase* ComponentBase_GetParent(const ComponentBase* component);
ComponentBase* ComponentBase_GetChild(const ComponentBase* component, int index);
int ComponentBase_GetChildCount(const ComponentBase* component);

// Event handling
void ComponentBase_SetClickCallback(ComponentBase* component, void (*callback)(ComponentBase*, Vector2));
void ComponentBase_SetHoverCallback(ComponentBase* component, void (*callback)(ComponentBase*, bool));
void ComponentBase_SetFocusCallback(ComponentBase* component, void (*callback)(ComponentBase*, bool));
void ComponentBase_SetKeyPressCallback(ComponentBase* component, void (*callback)(ComponentBase*, int));
void ComponentBase_SetTextInputCallback(ComponentBase* component, void (*callback)(ComponentBase*, const char*));

// State management
void ComponentBase_SetVisible(ComponentBase* component, bool visible);
void ComponentBase_SetInteractive(ComponentBase* component, bool interactive);
void ComponentBase_SetDisabled(ComponentBase* component, bool disabled);
void ComponentBase_SetFocused(ComponentBase* component, bool focused);
void ComponentBase_SetHovered(ComponentBase* component, bool hovered);
void ComponentBase_SetPressed(ComponentBase* component, bool pressed);

// Transform functions
void ComponentBase_SetTransform(ComponentBase* component, Vector2 offset, float rotation, Vector2 scale);
void ComponentBase_SetZIndex(ComponentBase* component, int z_index);

// Utility functions
bool ComponentBase_IsPointInside(const ComponentBase* component, Vector2 point);
bool ComponentBase_IsDescendantOf(const ComponentBase* component, const ComponentBase* ancestor);
ComponentBase* ComponentBase_FindChildById(const ComponentBase* component, unsigned int id);
ComponentBase* ComponentBase_FindChildByType(const ComponentBase* component, ComponentType type);

// Component ID generation
unsigned int ComponentBase_GenerateId(void);

#endif // COMPONENT_BASE_H
