#ifndef MARKUP_H
#define MARKUP_H

// Core systems for robustness
#include "components/error_handling.h"
#include "components/component_base.h"
#include "components/style_system.h"
#include "components/layout_engine.h"
#include "components/event_system.h"

// Theme system
#include "theme.h"

// UI Components
#include "components/button.h"
#include "components/panel.h"
#include "components/sidebar.h"
#include "components/slider.h"
#include "components/dropdown.h"
#include "components/text_input.h"
#include "components/text_area.h"
#include "components/checkbox.h"
#include "components/radio_button.h"
#include "components/modal.h"
#include "components/imagebox.h"
#include "components/separator.h"
#include "components/backdrop.h"
#include "components/blur_effect.h"
#include "components/shadow.h"
#include "components/loading_bar.h"
#include "components/spinner.h"
#include "components/tabs.h"
#include "components/toast.h"
#include "components/card.h"
#include "components/background_image.h"

// Main UI system context
typedef struct {
    // Core systems
    ErrorInfo last_error;
    StyleSystem style_system;
    EventSystem event_system;
    
    // Global state
    bool initialized;
    bool debug_mode;
    unsigned int frame_count;
    
    // Input state
    Vector2 mouse_position;
    Vector2 mouse_delta;
    bool mouse_buttons[3]; // Left, Right, Middle
    bool key_states[512];  // Key states
    char text_input_buffer[256];
    
    // Focus management
    ComponentBase* focused_component;
    ComponentBase* hovered_component;
    ComponentBase* pressed_component;
    
    // Root components
    ComponentBase* root_component;
    ComponentBase** modal_stack;
    int modal_count;
    int modal_capacity;
    
    // Performance tracking
    unsigned int components_updated;
    unsigned int components_drawn;
    float update_time;
    float draw_time;
} MarkupSystem;

// Main system functions
bool Markup_Init(MarkupSystem* system);
void Markup_Destroy(MarkupSystem* system);
void Markup_Update(MarkupSystem* system);
void Markup_Draw(MarkupSystem* system);

// System configuration
void Markup_SetDebugMode(MarkupSystem* system, bool debug_mode);
bool Markup_IsDebugMode(const MarkupSystem* system);
void Markup_SetRootComponent(MarkupSystem* system, ComponentBase* root);

// Input handling
void Markup_ProcessInput(MarkupSystem* system);
Vector2 Markup_GetMousePosition(const MarkupSystem* system);
bool Markup_IsMouseButtonPressed(const MarkupSystem* system, int button);
bool Markup_IsKeyPressed(const MarkupSystem* system, int key);
const char* Markup_GetTextInput(const MarkupSystem* system);

// Focus management
void Markup_SetFocusedComponent(MarkupSystem* system, ComponentBase* component);
ComponentBase* Markup_GetFocusedComponent(const MarkupSystem* system);
void Markup_ClearFocus(MarkupSystem* system);

// Modal management
bool Markup_ShowModal(MarkupSystem* system, ComponentBase* modal);
bool Markup_HideModal(MarkupSystem* system, ComponentBase* modal);
bool Markup_HideTopModal(MarkupSystem* system);
int Markup_GetModalCount(const MarkupSystem* system);

// Performance monitoring
unsigned int Markup_GetComponentsUpdated(const MarkupSystem* system);
unsigned int Markup_GetComponentsDrawn(const MarkupSystem* system);
float Markup_GetUpdateTime(const MarkupSystem* system);
float Markup_GetDrawTime(const MarkupSystem* system);

// Error handling
ErrorCode Markup_GetLastError(const MarkupSystem* system);
const char* Markup_GetLastErrorMessage(const MarkupSystem* system);
bool Markup_HasError(const MarkupSystem* system);
void Markup_ClearError(MarkupSystem* system);

// Utility functions
ComponentBase* Markup_FindComponentById(MarkupSystem* system, unsigned int id);
ComponentBase* Markup_FindComponentByType(MarkupSystem* system, ComponentType type);
bool Markup_IsComponentVisible(const ComponentBase* component);
bool Markup_IsComponentInteractive(const ComponentBase* component);

// Debug functions
void Markup_DebugPrintSystem(const MarkupSystem* system);
void Markup_DebugDrawBounds(const MarkupSystem* system);
void Markup_DebugPrintComponentTree(const ComponentBase* component, int depth);

// Global system instance (optional)
extern MarkupSystem g_markup_system;

// Convenience macros for global system
#define MARKUP_INIT() Markup_Init(&g_markup_system)
#define MARKUP_DESTROY() Markup_Destroy(&g_markup_system)
#define MARKUP_UPDATE() Markup_Update(&g_markup_system)
#define MARKUP_DRAW() Markup_Draw(&g_markup_system)

#endif // MARKUP_H