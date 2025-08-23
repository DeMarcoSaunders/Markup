#include "sidebar.h"
#include "components/button.h"
#include "components/panel.h"
#include "components/slider.h"
#include "shadow.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper function to get effective style values
static ComponentStyle* GetEffectiveStyle(Sidebar* sidebar) {
    return Theme_GetComponentStyle(COMPONENT_SIDEBAR);
}

// Helper function to calculate sidebar bounds based on position and state
static Rectangle CalculateSidebarBounds(Sidebar* sidebar) {
    Rectangle bounds = sidebar->bounds;
    float progress = sidebar->animation_progress;
    
    switch (sidebar->position) {
        case SIDEBAR_LEFT:
            bounds.width = bounds.width * progress;
            break;
        case SIDEBAR_RIGHT:
            bounds.x = bounds.x + bounds.width * (1.0f - progress);
            bounds.width = bounds.width * progress;
            break;
        case SIDEBAR_TOP:
            bounds.height = bounds.height * progress;
            break;
        case SIDEBAR_BOTTOM:
            bounds.y = bounds.y + bounds.height * (1.0f - progress);
            bounds.height = bounds.height * progress;
            break;
    }
    
    return bounds;
}

// Helper function to layout children
static void LayoutSidebarChildren(Sidebar* sidebar) {
    if (!sidebar || sidebar->children.length == 0) return;
    
    ComponentStyle* style = GetEffectiveStyle(sidebar);
    EdgeValues padding = sidebar->custom_padding ? *sidebar->custom_padding : style->padding;
    
    Rectangle content_area = sidebar->bounds;
    content_area.x += padding.left;
    content_area.y += padding.top;
    content_area.width -= (padding.left + padding.right);
    content_area.height -= (padding.top + padding.bottom);
    
    float current_pos = 0;
    float total_flex_grow = 0;
    float total_fixed_size = 0;
    
    // Calculate total flex grow and fixed sizes
    for (int i = 0; i < sidebar->children.length; i++) {
        SidebarChild* child = &sidebar->children.data[i];
        if (!child->is_visible) continue;
        
        if (child->flex_grow > 0) {
            total_flex_grow += child->flex_grow;
        } else {
            total_fixed_size += child->flex_basis;
        }
    }
    
    // Calculate available space for flex items
    float available_space = 0;
    if (sidebar->flex_direction == FLEX_DIRECTION_COLUMN) {
        available_space = content_area.height - total_fixed_size - (sidebar->children.length - 1) * sidebar->gap;
    } else {
        available_space = content_area.width - total_fixed_size - (sidebar->children.length - 1) * sidebar->gap;
    }
    
    // Layout children
    for (int i = 0; i < sidebar->children.length; i++) {
        SidebarChild* child = &sidebar->children.data[i];
        if (!child->is_visible) continue;
        
        float child_size = 0;
        if (child->flex_grow > 0) {
            child_size = (available_space / total_flex_grow) * child->flex_grow;
        } else {
            child_size = child->flex_basis;
        }
        
        // Set component bounds
        if (sidebar->flex_direction == FLEX_DIRECTION_COLUMN) {
            if (child->type == SIDEBAR_CHILD_BUTTON) {
                Button* button = (Button*)child->component;
                button->rect.x = content_area.x + child->margin.left;
                button->rect.y = content_area.y + current_pos + child->margin.top;
                button->rect.width = content_area.width - child->margin.left - child->margin.right;
                button->rect.height = child_size - child->margin.top - child->margin.bottom;
            } else if (child->type == SIDEBAR_CHILD_PANEL) {
                Panel* panel = (Panel*)child->component;
                panel->rect.x = content_area.x + child->margin.left;
                panel->rect.y = content_area.y + current_pos + child->margin.top;
                panel->rect.width = content_area.width - child->margin.left - child->margin.right;
                panel->rect.height = child_size - child->margin.top - child->margin.bottom;
            } else if (child->type == SIDEBAR_CHILD_SLIDER) {
                Slider* slider = (Slider*)child->component;
                slider->bounds.x = content_area.x + child->margin.left;
                slider->bounds.y = content_area.y + current_pos + child->margin.top;
                slider->bounds.width = content_area.width - child->margin.left - child->margin.right;
                slider->bounds.height = child_size - child->margin.top - child->margin.bottom;
            }
            current_pos += child_size + sidebar->gap;
        } else {
            if (child->type == SIDEBAR_CHILD_BUTTON) {
                Button* button = (Button*)child->component;
                button->rect.x = content_area.x + current_pos + child->margin.left;
                button->rect.y = content_area.y + child->margin.top;
                button->rect.width = child_size - child->margin.left - child->margin.right;
                button->rect.height = content_area.height - child->margin.top - child->margin.bottom;
            } else if (child->type == SIDEBAR_CHILD_PANEL) {
                Panel* panel = (Panel*)child->component;
                panel->rect.x = content_area.x + current_pos + child->margin.left;
                panel->rect.y = content_area.y + child->margin.top;
                panel->rect.width = child_size - child->margin.left - child->margin.right;
                panel->rect.height = content_area.height - child->margin.top - child->margin.bottom;
            } else if (child->type == SIDEBAR_CHILD_SLIDER) {
                Slider* slider = (Slider*)child->component;
                slider->bounds.x = content_area.x + current_pos + child->margin.left;
                slider->bounds.y = content_area.y + child->margin.top;
                slider->bounds.width = child_size - child->margin.left - child->margin.right;
                slider->bounds.height = content_area.height - child->margin.top - child->margin.bottom;
            }
            current_pos += child_size + sidebar->gap;
        }
    }
}

Sidebar Sidebar_Create(Rectangle bounds, SidebarPosition position) {
    Sidebar sidebar = {0};
    
    sidebar.bounds = bounds;
    sidebar.position = position;
    sidebar.state = SIDEBAR_STATE_EXPANDED;
    
    // Initialize layout properties
    sidebar.flex_direction = FLEX_DIRECTION_COLUMN;
    sidebar.justify_content = JUSTIFY_START;
    sidebar.align_items = ALIGN_STRETCH;
    sidebar.gap = 10.0f;
    
    // Initialize animation
    sidebar.animation_progress = 1.0f;
    sidebar.animation_speed = 8.0f;
    sidebar.is_animating = false;
    
    // Initialize children vector
    vec_init(&sidebar.children);
    
    // Initialize visual properties
    sidebar.show_border = true;
    sidebar.show_shadow = false;
    sidebar.is_visible = true;
    sidebar.is_interactive = true;
    sidebar.z_index = 5;
    
    // Initialize custom styling pointers to NULL
    sidebar.custom_background_color = NULL;
    sidebar.custom_border_color = NULL;
    sidebar.custom_padding = NULL;
    sidebar.custom_margin = NULL;
    sidebar.custom_border_radius = NULL;
    
    // Initialize callbacks
    sidebar.on_state_changed = NULL;
    sidebar.user_data = NULL;
    
    return sidebar;
}

void Sidebar_Destroy(Sidebar* sidebar) {
    if (!sidebar) return;
    
    // Clear children
    Sidebar_ClearChildren(sidebar);
    vec_deinit(&sidebar->children);
    
    // Free custom styling
    if (sidebar->custom_background_color) { free(sidebar->custom_background_color); sidebar->custom_background_color = NULL; }
    if (sidebar->custom_border_color) { free(sidebar->custom_border_color); sidebar->custom_border_color = NULL; }
    if (sidebar->custom_padding) { free(sidebar->custom_padding); sidebar->custom_padding = NULL; }
    if (sidebar->custom_margin) { free(sidebar->custom_margin); sidebar->custom_margin = NULL; }
    if (sidebar->custom_border_radius) { free(sidebar->custom_border_radius); sidebar->custom_border_radius = NULL; }
}

void Sidebar_Update(Sidebar* sidebar) {
    if (!sidebar || !sidebar->is_visible) return;
    
    // Update animation
    if (sidebar->is_animating) {
        float target_progress = (sidebar->state == SIDEBAR_STATE_EXPANDED) ? 1.0f : 0.0f;
        float diff = target_progress - sidebar->animation_progress;
        
        if (fabsf(diff) < 0.01f) {
            sidebar->animation_progress = target_progress;
            sidebar->is_animating = false;
            sidebar->state = (target_progress > 0.5f) ? SIDEBAR_STATE_EXPANDED : SIDEBAR_STATE_COLLAPSED;
            
            if (sidebar->on_state_changed) {
                sidebar->on_state_changed(sidebar->state, sidebar->user_data);
            }
        } else {
            sidebar->animation_progress += diff * sidebar->animation_speed * GetFrameTime();
        }
    }
    
    // Update children
    for (int i = 0; i < sidebar->children.length; i++) {
        SidebarChild* child = &sidebar->children.data[i];
        if (!child->is_visible || !child->is_interactive) continue;
        
        switch (child->type) {
            case SIDEBAR_CHILD_BUTTON:
                Button_Update((Button*)child->component);
                break;
            case SIDEBAR_CHILD_PANEL:
                Panel_Update((Panel*)child->component);
                break;
            case SIDEBAR_CHILD_SLIDER:
                // Slider_Update((Slider*)child->component);
                break;
            default:
                break;
        }
    }
    
    // Layout children
    LayoutSidebarChildren(sidebar);
}

void Sidebar_Draw(const Sidebar* sidebar) {
    if (!sidebar || !sidebar->is_visible) return;
    
    ComponentStyle* style = Theme_GetComponentStyle(COMPONENT_SIDEBAR);
    Rectangle render_bounds = CalculateSidebarBounds((Sidebar*)sidebar);
    
    // Get effective values
    Color bg_color = sidebar->custom_background_color ? *sidebar->custom_background_color : style->colors[STATE_DEFAULT].background;
    Color border_color = sidebar->custom_border_color ? *sidebar->custom_border_color : style->colors[STATE_DEFAULT].border;
    EdgeValues padding = sidebar->custom_padding ? *sidebar->custom_padding : style->padding;
    CornerRadius border_radius = sidebar->custom_border_radius ? *sidebar->custom_border_radius : style->border_radius;
    
    // Draw shadow
    if (sidebar->show_shadow) {
        ShadowConfig shadow_config = Shadow_FromTheme(style, STATE_DEFAULT);
        Shadow_DrawRectangleRounded(render_bounds, border_radius.top_left, &shadow_config);
    }
    
    // Draw background
    DrawRectangleRounded(render_bounds, border_radius.top_left, 8, bg_color);
    
    // Draw border
    if (sidebar->show_border) {
        DrawRectangleRoundedLines(render_bounds, border_radius.top_left, 8, border_color);
    }
    
    // Draw children
    for (int i = 0; i < sidebar->children.length; i++) {
        const SidebarChild* child = &sidebar->children.data[i];
        if (!child->is_visible) continue;
        
        switch (child->type) {
            case SIDEBAR_CHILD_BUTTON:
                Button_Draw((Button*)child->component);
                break;
            case SIDEBAR_CHILD_PANEL:
                Panel_Draw((Panel*)child->component);
                break;
            case SIDEBAR_CHILD_SLIDER:
                // Slider_Draw((Slider*)child->component);
                break;
            default:
                break;
        }
    }
}

void Sidebar_SetState(Sidebar* sidebar, SidebarState state) {
    if (!sidebar) return;
    
    if (state != sidebar->state) {
        sidebar->state = state;
        sidebar->is_animating = true;
        
        if (sidebar->on_state_changed) {
            sidebar->on_state_changed(state, sidebar->user_data);
        }
    }
}

SidebarState Sidebar_GetState(const Sidebar* sidebar) {
    return sidebar ? sidebar->state : SIDEBAR_STATE_COLLAPSED;
}

void Sidebar_Toggle(Sidebar* sidebar) {
    if (!sidebar) return;
    
    if (sidebar->state == SIDEBAR_STATE_EXPANDED) {
        Sidebar_Collapse(sidebar);
    } else {
        Sidebar_Expand(sidebar);
    }
}

void Sidebar_Expand(Sidebar* sidebar) {
    Sidebar_SetState(sidebar, SIDEBAR_STATE_EXPANDED);
}

void Sidebar_Collapse(Sidebar* sidebar) {
    Sidebar_SetState(sidebar, SIDEBAR_STATE_COLLAPSED);
}

void Sidebar_SetFlexDirection(Sidebar* sidebar, FlexDirection direction) {
    if (sidebar) sidebar->flex_direction = direction;
}

void Sidebar_SetJustifyContent(Sidebar* sidebar, JustifyContent justify) {
    if (sidebar) sidebar->justify_content = justify;
}

void Sidebar_SetAlignItems(Sidebar* sidebar, AlignItems align) {
    if (sidebar) sidebar->align_items = align;
}

void Sidebar_SetGap(Sidebar* sidebar, float gap) {
    if (sidebar) sidebar->gap = gap;
}

int Sidebar_AddButton(Sidebar* sidebar, void* button) {
    if (!sidebar || !button) return -1;
    
    SidebarChild child = {0};
    child.type = SIDEBAR_CHILD_BUTTON;
    child.component = button;
    child.is_visible = true;
    child.is_interactive = true;
    child.flex_grow = 0;
    child.flex_shrink = 1;
    child.flex_basis = 40.0f;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    
    vec_push(&sidebar->children, child);
    return sidebar->children.length - 1;
}

int Sidebar_AddPanel(Sidebar* sidebar, void* panel) {
    if (!sidebar || !panel) return -1;
    
    SidebarChild child = {0};
    child.type = SIDEBAR_CHILD_PANEL;
    child.component = panel;
    child.is_visible = true;
    child.is_interactive = true;
    child.flex_grow = 1;
    child.flex_shrink = 1;
    child.flex_basis = 0.0f;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    
    vec_push(&sidebar->children, child);
    return sidebar->children.length - 1;
}

int Sidebar_AddSlider(Sidebar* sidebar, void* slider) {
    if (!sidebar || !slider) return -1;
    
    SidebarChild child = {0};
    child.type = SIDEBAR_CHILD_SLIDER;
    child.component = slider;
    child.is_visible = true;
    child.is_interactive = true;
    child.flex_grow = 0;
    child.flex_shrink = 1;
    child.flex_basis = 30.0f;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    
    vec_push(&sidebar->children, child);
    return sidebar->children.length - 1;
}

int Sidebar_AddCustom(Sidebar* sidebar, void* component, SidebarChildType type) {
    if (!sidebar || !component) return -1;
    
    SidebarChild child = {0};
    child.type = type;
    child.component = component;
    child.is_visible = true;
    child.is_interactive = true;
    child.flex_grow = 0;
    child.flex_shrink = 1;
    child.flex_basis = 40.0f;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    
    vec_push(&sidebar->children, child);
    return sidebar->children.length - 1;
}

void Sidebar_RemoveChild(Sidebar* sidebar, int index) {
    if (!sidebar || index < 0 || index >= sidebar->children.length) return;
    
    vec_splice(&sidebar->children, index, 1);
}

void Sidebar_ClearChildren(Sidebar* sidebar) {
    if (!sidebar) return;
    
    vec_clear(&sidebar->children);
}

void Sidebar_SetChildFlexGrow(Sidebar* sidebar, int child_index, int flex_grow) {
    if (!sidebar || child_index < 0 || child_index >= sidebar->children.length) return;
    sidebar->children.data[child_index].flex_grow = flex_grow;
}

void Sidebar_SetChildFlexShrink(Sidebar* sidebar, int child_index, int flex_shrink) {
    if (!sidebar || child_index < 0 || child_index >= sidebar->children.length) return;
    sidebar->children.data[child_index].flex_shrink = flex_shrink;
}

void Sidebar_SetChildFlexBasis(Sidebar* sidebar, int child_index, float flex_basis) {
    if (!sidebar || child_index < 0 || child_index >= sidebar->children.length) return;
    sidebar->children.data[child_index].flex_basis = flex_basis;
}

void Sidebar_SetChildMargin(Sidebar* sidebar, int child_index, EdgeValues margin) {
    if (!sidebar || child_index < 0 || child_index >= sidebar->children.length) return;
    sidebar->children.data[child_index].margin = margin;
}

void Sidebar_SetChildVisible(Sidebar* sidebar, int child_index, bool visible) {
    if (!sidebar || child_index < 0 || child_index >= sidebar->children.length) return;
    sidebar->children.data[child_index].is_visible = visible;
}

void Sidebar_SetChildInteractive(Sidebar* sidebar, int child_index, bool interactive) {
    if (!sidebar || child_index < 0 || child_index >= sidebar->children.length) return;
    sidebar->children.data[child_index].is_interactive = interactive;
}

void Sidebar_SetBorder(Sidebar* sidebar, bool show_border) {
    if (sidebar) sidebar->show_border = show_border;
}

void Sidebar_SetShadow(Sidebar* sidebar, bool show_shadow) {
    if (sidebar) sidebar->show_shadow = show_shadow;
}

void Sidebar_SetVisible(Sidebar* sidebar, bool visible) {
    if (sidebar) sidebar->is_visible = visible;
}

void Sidebar_SetInteractive(Sidebar* sidebar, bool interactive) {
    if (sidebar) sidebar->is_interactive = interactive;
}

void Sidebar_SetZIndex(Sidebar* sidebar, int z_index) {
    if (sidebar) sidebar->z_index = z_index;
}

void Sidebar_SetBackgroundColor(Sidebar* sidebar, Color color) {
    if (!sidebar) return;
    
    if (!sidebar->custom_background_color) {
        sidebar->custom_background_color = (Color*)malloc(sizeof(Color));
    }
    
    if (sidebar->custom_background_color) {
        *sidebar->custom_background_color = color;
    }
}

void Sidebar_SetBorderColor(Sidebar* sidebar, Color color) {
    if (!sidebar) return;
    
    if (!sidebar->custom_border_color) {
        sidebar->custom_border_color = (Color*)malloc(sizeof(Color));
    }
    
    if (sidebar->custom_border_color) {
        *sidebar->custom_border_color = color;
    }
}

void Sidebar_SetPadding(Sidebar* sidebar, float top, float right, float bottom, float left) {
    if (!sidebar) return;
    
    if (!sidebar->custom_padding) {
        sidebar->custom_padding = (EdgeValues*)malloc(sizeof(EdgeValues));
    }
    
    if (sidebar->custom_padding) {
        sidebar->custom_padding->top = top;
        sidebar->custom_padding->right = right;
        sidebar->custom_padding->bottom = bottom;
        sidebar->custom_padding->left = left;
    }
}

void Sidebar_SetMargin(Sidebar* sidebar, float top, float right, float bottom, float left) {
    if (!sidebar) return;
    
    if (!sidebar->custom_margin) {
        sidebar->custom_margin = (EdgeValues*)malloc(sizeof(EdgeValues));
    }
    
    if (sidebar->custom_margin) {
        sidebar->custom_margin->top = top;
        sidebar->custom_margin->right = right;
        sidebar->custom_margin->bottom = bottom;
        sidebar->custom_margin->left = left;
    }
}

void Sidebar_SetBorderRadius(Sidebar* sidebar, float top_left, float top_right, float bottom_right, float bottom_left) {
    if (!sidebar) return;
    
    if (!sidebar->custom_border_radius) {
        sidebar->custom_border_radius = (CornerRadius*)malloc(sizeof(CornerRadius));
    }
    
    if (sidebar->custom_border_radius) {
        sidebar->custom_border_radius->top_left = top_left;
        sidebar->custom_border_radius->top_right = top_right;
        sidebar->custom_border_radius->bottom_right = bottom_right;
        sidebar->custom_border_radius->bottom_left = bottom_left;
    }
}

void Sidebar_SetAnimationSpeed(Sidebar* sidebar, float speed) {
    if (sidebar) sidebar->animation_speed = speed;
}

float Sidebar_GetAnimationProgress(const Sidebar* sidebar) {
    return sidebar ? sidebar->animation_progress : 0.0f;
}

bool Sidebar_IsAnimating(const Sidebar* sidebar) {
    return sidebar ? sidebar->is_animating : false;
}

void Sidebar_SetOnStateChanged(Sidebar* sidebar, void (*callback)(SidebarState, void*), void* user_data) {
    if (!sidebar) return;
    
    sidebar->on_state_changed = callback;
    sidebar->user_data = user_data;
}

Rectangle Sidebar_GetBounds(const Sidebar* sidebar) {
    return sidebar ? CalculateSidebarBounds((Sidebar*)sidebar) : (Rectangle){0, 0, 0, 0};
}

bool Sidebar_IsExpanded(const Sidebar* sidebar) {
    return sidebar ? (sidebar->state == SIDEBAR_STATE_EXPANDED) : false;
}

bool Sidebar_IsCollapsed(const Sidebar* sidebar) {
    return sidebar ? (sidebar->state == SIDEBAR_STATE_COLLAPSED) : false;
}

int Sidebar_GetChildCount(const Sidebar* sidebar) {
    return sidebar ? sidebar->children.length : 0;
}

void Sidebar_ResetCustomStyles(Sidebar* sidebar) {
    if (!sidebar) return;
    
    // Free all custom style allocations
    if (sidebar->custom_background_color) { free(sidebar->custom_background_color); sidebar->custom_background_color = NULL; }
    if (sidebar->custom_border_color) { free(sidebar->custom_border_color); sidebar->custom_border_color = NULL; }
    if (sidebar->custom_padding) { free(sidebar->custom_padding); sidebar->custom_padding = NULL; }
    if (sidebar->custom_margin) { free(sidebar->custom_margin); sidebar->custom_margin = NULL; }
    if (sidebar->custom_border_radius) { free(sidebar->custom_border_radius); sidebar->custom_border_radius = NULL; }
} 