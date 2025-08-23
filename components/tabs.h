#ifndef TABS_H
#define TABS_H

#include "raylib.h"
#include "component_base.h"
#include <stdbool.h>

// Tab styles
typedef enum {
    TABS_STYLE_LINEAR,
    TABS_STYLE_PILLS,
    TABS_STYLE_CARDS,
    TABS_STYLE_UNDERLINE
} TabsStyle;

// Tab alignment
typedef enum {
    TABS_ALIGN_LEFT,
    TABS_ALIGN_CENTER,
    TABS_ALIGN_RIGHT,
    TABS_ALIGN_JUSTIFY
} TabsAlignment;

typedef struct Tab {
    char title[256];
    char id[64];
    bool is_active;
    bool is_disabled;
    Rectangle rect;
    ComponentBase* content;
    void* user_data;
} Tab;

typedef struct {
    // Base component
    ComponentBase base;
    
    // Tab properties
    Tab* tabs;
    int tab_count;
    int tab_capacity;
    int active_tab_index;
    
    // Visual properties
    TabsStyle style;
    TabsAlignment alignment;
    
    // Colors
    Color background_color;
    Color active_tab_color;
    Color inactive_tab_color;
    Color disabled_tab_color;
    Color border_color;
    Color text_color;
    Color active_text_color;
    Color disabled_text_color;
    
    // Dimensions
    float tab_height;
    float tab_padding;
    float tab_spacing;
    float border_width;
    float corner_radius;
    
    // Content area
    Rectangle content_rect;
    bool show_content_border;
    Color content_border_color;
    
    // Animation
    float animation_progress;
    float animation_speed;
    bool is_animating;
    
    // Callbacks
    void (*on_tab_change)(struct Tabs* tabs, int old_index, int new_index);
    void (*on_tab_click)(struct Tabs* tabs, int tab_index);
} Tabs;

// Core Functions
Tabs Tabs_Create(Rectangle rect, TabsStyle style);
bool Tabs_Init(Tabs* tabs, Rectangle rect, TabsStyle style);
void Tabs_Update(Tabs* tabs);
void Tabs_Draw(const Tabs* tabs);
void Tabs_Destroy(Tabs* tabs);

// Tab Management
int Tabs_AddTab(Tabs* tabs, const char* title, const char* id);
bool Tabs_RemoveTab(Tabs* tabs, int index);
bool Tabs_RemoveTabById(Tabs* tabs, const char* id);
void Tabs_ClearTabs(Tabs* tabs);
int Tabs_GetTabCount(const Tabs* tabs);

// Tab Content
bool Tabs_SetTabContent(Tabs* tabs, int index, ComponentBase* content);
ComponentBase* Tabs_GetTabContent(const Tabs* tabs, int index);
bool Tabs_SetTabContentById(Tabs* tabs, const char* id, ComponentBase* content);

// Tab State
bool Tabs_SetActiveTab(Tabs* tabs, int index);
bool Tabs_SetActiveTabById(Tabs* tabs, const char* id);
int Tabs_GetActiveTabIndex(const Tabs* tabs);
const char* Tabs_GetActiveTabId(const Tabs* tabs);
bool Tabs_IsTabActive(const Tabs* tabs, int index);

// Tab Properties
bool Tabs_SetTabTitle(Tabs* tabs, int index, const char* title);
bool Tabs_SetTabTitleById(Tabs* tabs, const char* id, const char* title);
const char* Tabs_GetTabTitle(const Tabs* tabs, int index);
bool Tabs_SetTabDisabled(Tabs* tabs, int index, bool disabled);
bool Tabs_SetTabDisabledById(Tabs* tabs, const char* id, bool disabled);
bool Tabs_IsTabDisabled(const Tabs* tabs, int index);

// Style Configuration
void Tabs_SetStyle(Tabs* tabs, TabsStyle style);
void Tabs_SetAlignment(Tabs* tabs, TabsAlignment alignment);
TabsStyle Tabs_GetStyle(const Tabs* tabs);
TabsAlignment Tabs_GetAlignment(const Tabs* tabs);

// Visual Customization
void Tabs_SetColors(Tabs* tabs, Color background, Color active, Color inactive, Color disabled);
void Tabs_SetBackgroundColor(Tabs* tabs, Color color);
void Tabs_SetActiveTabColor(Tabs* tabs, Color color);
void Tabs_SetInactiveTabColor(Tabs* tabs, Color color);
void Tabs_SetDisabledTabColor(Tabs* tabs, Color color);
void Tabs_SetBorderColor(Tabs* tabs, Color color);
void Tabs_SetTextColor(Tabs* tabs, Color color);
void Tabs_SetActiveTextColor(Tabs* tabs, Color color);
void Tabs_SetDisabledTextColor(Tabs* tabs, Color color);

// Dimensions
void Tabs_SetTabHeight(Tabs* tabs, float height);
void Tabs_SetTabPadding(Tabs* tabs, float padding);
void Tabs_SetTabSpacing(Tabs* tabs, float spacing);
void Tabs_SetBorderWidth(Tabs* tabs, float width);
void Tabs_SetCornerRadius(Tabs* tabs, float radius);

// Content Area
void Tabs_SetShowContentBorder(Tabs* tabs, bool show);
void Tabs_SetContentBorderColor(Tabs* tabs, Color color);
Rectangle Tabs_GetContentRect(const Tabs* tabs);

// Animation
void Tabs_SetAnimationSpeed(Tabs* tabs, float speed);
void Tabs_EnableAnimation(Tabs* tabs, bool enable);
bool Tabs_IsAnimating(const Tabs* tabs);

// Callbacks
void Tabs_SetOnTabChangeCallback(Tabs* tabs, void (*callback)(Tabs* tabs, int old_index, int new_index));
void Tabs_SetOnTabClickCallback(Tabs* tabs, void (*callback)(Tabs* tabs, int tab_index));

// Utility Functions
int Tabs_GetTabIndexById(const Tabs* tabs, const char* id);
const char* Tabs_GetTabId(const Tabs* tabs, int index);
bool Tabs_IsValidTabIndex(const Tabs* tabs, int index);
bool Tabs_IsValidTabId(const Tabs* tabs, const char* id);

// Input Handling
bool Tabs_HandleMouseClick(Tabs* tabs, Vector2 position);
bool Tabs_HandleMouseHover(Tabs* tabs, Vector2 position);

#endif // TABS_H
