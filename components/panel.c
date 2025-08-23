#include "panel.h"
#include "components/button.h"
#include "components/sidebar.h"
#include "theme.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

Panel Panel_Create(Rectangle rect) {
    Panel panel = {0};
    panel.rect = rect;
    panel.is_visible = true;
    
    // Initialize children array
    vec_init(&panel.children);
    
    // Set default flexbox properties
    panel.flex_direction = FLEX_DIRECTION_COLUMN;
    panel.justify_content = JUSTIFY_START;
    panel.align_items = ALIGN_START;
    panel.gap = 0.0f;
    
    // Initialize layout cache
    panel.content_area = rect;
    panel.needs_layout_recalculation = true;
    
    // Initialize layout and positioning
    panel.z_index = AppTheme.components[COMPONENT_PANEL].z_index;
    panel.clip_content = true;  // Enable clipping by default
    
    return panel;
}

void Panel_Update(Panel* panel) {
    if (panel == NULL || !panel->is_visible) return;
    
    // Recalculate layout if needed
    if (panel->needs_layout_recalculation) {
        Panel_RecalculateLayout(panel);
    }
    
    // Get mouse position for input delegation
    Vector2 mouse_pos = GetMousePosition();
    bool mouse_in_panel = CheckCollisionPointRec(mouse_pos, panel->content_area);
    
    // Update child components with input delegation
    for (int i = 0; i < panel->children.length; i++) {
        PanelChild* child = &panel->children.data[i];
        if (!child->is_visible || !child->is_interactive) continue;
        
        // Check if mouse is within child bounds for proper input delegation
        bool mouse_in_child = mouse_in_panel && CheckCollisionPointRec(mouse_pos, child->calculated_rect);
        
        // Delegate input to child components based on their type
        switch (child->type) {
            case PANEL_CHILD_BUTTON:
                if (child->component) {
                    // Only update button if mouse is in panel area or button doesn't need mouse input
                    Button_Update((Button*)child->component);
                }
                break;
            case PANEL_CHILD_PANEL:
                if (child->component) {
                    // Nested panels always get updated for proper layout recalculation
                    Panel_Update((Panel*)child->component);
                }
                break;
            case PANEL_CHILD_SLIDER:
                // TODO: Implement when slider is available
                // if (child->component) {
                //     Slider_Update((Slider*)child->component);
                // }
                break;
            case PANEL_CHILD_SIDEBAR:
                if (child->component) {
                    Sidebar_Update((Sidebar*)child->component);
                }
                break;
            case PANEL_CHILD_CUSTOM:
                // Custom components need their own update handling
                // Application code should handle custom component updates
                break;
        }
    }
}

void Panel_Draw(const Panel* panel) {
    if (panel == NULL || !panel->is_visible) return;
    
    // Draw panel background using theme color
    ComponentStyle* style = &AppTheme.components[COMPONENT_PANEL];
    DrawRectangleRec(panel->rect, style->colors[STATE_DEFAULT].background);
    
    // Draw panel border/outline
    DrawRectangleLinesEx(panel->rect, style->border_width.top, style->colors[STATE_DEFAULT].border);
    
    // Enable clipping to panel content area if requested
    bool clipping_enabled = panel->clip_content;
    if (clipping_enabled) {
        BeginScissorMode((int)panel->content_area.x, (int)panel->content_area.y, 
                         (int)panel->content_area.width, (int)panel->content_area.height);
    }
    
    // Create array of child indices for z-index sorting
    typedef struct {
        int index;
        int z_index;
    } ChildZIndex;
    
    ChildZIndex* child_z_indices = (ChildZIndex*)malloc(panel->children.length * sizeof(ChildZIndex));
    int visible_count = 0;
    
    // Collect visible children with their z-indices
    for (int i = 0; i < panel->children.length; i++) {
        PanelChild* child = &panel->children.data[i];
        if (!child->is_visible) continue;
        
        child_z_indices[visible_count].index = i;
        
        // Get z-index from child component (default to 0 if not available)
        int z_index = 0;
        switch (child->type) {
            case PANEL_CHILD_BUTTON:
                if (child->component) {
                    z_index = ((Button*)child->component)->z_index;
                }
                break;
            case PANEL_CHILD_PANEL:
                if (child->component) {
                    z_index = ((Panel*)child->component)->z_index;
                }
                break;
            case PANEL_CHILD_SLIDER:
                if (child->component) {
                    // z_index = ((Slider*)child->component)->z_index; // TODO
                }
                break;
            case PANEL_CHILD_SIDEBAR:
                 if (child->component) {
                    z_index = ((Sidebar*)child->component)->z_index;
                }
                break;
            case PANEL_CHILD_CUSTOM:
                // Custom components default to 0
                z_index = 0;
                break;
        }
        
        child_z_indices[visible_count].z_index = z_index;
        visible_count++;
    }
    
    // Simple bubble sort by z_index (low to high for back-to-front rendering)
    for (int i = 0; i < visible_count - 1; i++) {
        for (int j = 0; j < visible_count - i - 1; j++) {
            if (child_z_indices[j].z_index > child_z_indices[j + 1].z_index) {
                ChildZIndex temp = child_z_indices[j];
                child_z_indices[j] = child_z_indices[j + 1];
                child_z_indices[j + 1] = temp;
            }
        }
    }
    
    // Draw child components in z-index order
    for (int i = 0; i < visible_count; i++) {
        int child_index = child_z_indices[i].index;
        PanelChild* child = &panel->children.data[child_index];
        
        // Draw child components based on their type
        switch (child->type) {
            case PANEL_CHILD_BUTTON:
                if (child->component) {
                    Button_Draw((Button*)child->component);
                }
                break;
            case PANEL_CHILD_PANEL:
                if (child->component) {
                    Panel_Draw((Panel*)child->component);
                }
                break;
            case PANEL_CHILD_SLIDER:
                // TODO: Implement when slider is available
                break;
            case PANEL_CHILD_SIDEBAR:
                if (child->component) {
                    Sidebar_Draw((Sidebar*)child->component);
                }
                break;
            case PANEL_CHILD_CUSTOM:
                // Custom components need their own draw handling
                break;
        }
    }
    
    free(child_z_indices);
    
    // Disable clipping if it was enabled
    if (clipping_enabled) {
        EndScissorMode();
    }
}

void Panel_Destroy(Panel* panel) {
    if (panel == NULL) return;
    
    // Clean up children array
    vec_deinit(&panel->children);
}

// Child Component Management Functions
int Panel_AddButton(Panel* panel, Button* button, int flex_grow, float flex_basis) {
    if (panel == NULL || button == NULL) return -1;
    
    PanelChild child = {0};
    child.type = PANEL_CHILD_BUTTON;
    child.component = button;
    child.flex_grow = flex_grow;
    child.flex_shrink = 1;
    child.flex_basis = flex_basis;
    child.align_self = ALIGN_START; // Use parent's align_items by default
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    child.is_visible = true;
    child.is_interactive = true;
    child.needs_layout_update = true;
    
    vec_push(&panel->children, child);
    
    panel->needs_layout_recalculation = true;
    return panel->children.length - 1; // Return index of added child
}

int Panel_AddPanel(Panel* panel, Panel* child_panel, int flex_grow, float flex_basis) {
    if (panel == NULL || child_panel == NULL) return -1;
    
    PanelChild child = {0};
    child.type = PANEL_CHILD_PANEL;
    child.component = child_panel;
    child.flex_grow = flex_grow;
    child.flex_shrink = 1;
    child.flex_basis = flex_basis;
    child.align_self = ALIGN_START;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    child.is_visible = true;
    child.is_interactive = true;
    child.needs_layout_update = true;
    
    vec_push(&panel->children, child);
    
    panel->needs_layout_recalculation = true;
    return panel->children.length - 1;
}

int Panel_AddSlider(Panel* panel, void* slider, int flex_grow, float flex_basis) {
    if (panel == NULL || slider == NULL) return -1;
    
    PanelChild child = {0};
    child.type = PANEL_CHILD_SLIDER;
    child.component = slider;
    child.flex_grow = flex_grow;
    child.flex_shrink = 1;
    child.flex_basis = flex_basis;
    child.align_self = ALIGN_START;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    child.is_visible = true;
    child.is_interactive = true;
    child.needs_layout_update = true;
    
    vec_push(&panel->children, child);
    
    panel->needs_layout_recalculation = true;
    return panel->children.length - 1;
}

int Panel_AddSidebar(Panel* panel, Sidebar* sidebar, int flex_grow, float flex_basis) {
    if (panel == NULL || sidebar == NULL) return -1;
    
    PanelChild child = {0};
    child.type = PANEL_CHILD_SIDEBAR;
    child.component = sidebar;
    child.flex_grow = flex_grow;
    child.flex_shrink = 1;
    child.flex_basis = flex_basis;
    child.align_self = ALIGN_START;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    child.is_visible = true;
    child.is_interactive = true;
    child.needs_layout_update = true;
    
    vec_push(&panel->children, child);
    
    panel->needs_layout_recalculation = true;
    return panel->children.length - 1;
}

int Panel_AddCustom(Panel* panel, void* component, int flex_grow, float flex_basis) {
    if (panel == NULL || component == NULL) return -1;
    
    PanelChild child = {0};
    child.type = PANEL_CHILD_CUSTOM;
    child.component = component;
    child.flex_grow = flex_grow;
    child.flex_shrink = 1;
    child.flex_basis = flex_basis;
    child.align_self = ALIGN_START;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    child.is_visible = true;
    child.is_interactive = true;
    child.needs_layout_update = true;
    
    vec_push(&panel->children, child);
    
    panel->needs_layout_recalculation = true;
    return panel->children.length - 1;
}

int Panel_AddDropdown(Panel* panel, void* dropdown, int flex_grow, float flex_basis) {
    if (panel == NULL || dropdown == NULL) return -1;
    
    PanelChild child = {0};
    child.type = PANEL_CHILD_CUSTOM; // Use custom type for dropdown
    child.component = dropdown;
    child.flex_grow = flex_grow;
    child.flex_shrink = 1;
    child.flex_basis = flex_basis;
    child.align_self = ALIGN_START;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    child.is_visible = true;
    child.is_interactive = true;
    child.needs_layout_update = true;
    
    vec_push(&panel->children, child);
    
    panel->needs_layout_recalculation = true;
    return panel->children.length - 1;
}

int Panel_AddTextInput(Panel* panel, void* text_input, int flex_grow, float flex_basis) {
    if (panel == NULL || text_input == NULL) return -1;
    
    PanelChild child = {0};
    child.type = PANEL_CHILD_CUSTOM; // Use custom type for text input
    child.component = text_input;
    child.flex_grow = flex_grow;
    child.flex_shrink = 1;
    child.flex_basis = flex_basis;
    child.align_self = ALIGN_START;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    child.is_visible = true;
    child.is_interactive = true;
    child.needs_layout_update = true;
    
    vec_push(&panel->children, child);
    
    panel->needs_layout_recalculation = true;
    return panel->children.length - 1;
}

int Panel_AddTextArea(Panel* panel, void* text_area, int flex_grow, float flex_basis) {
    if (panel == NULL || text_area == NULL) return -1;
    
    PanelChild child = {0};
    child.type = PANEL_CHILD_CUSTOM; // Use custom type for text area
    child.component = text_area;
    child.flex_grow = flex_grow;
    child.flex_shrink = 1;
    child.flex_basis = flex_basis;
    child.align_self = ALIGN_START;
    child.margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    child.is_visible = true;
    child.is_interactive = true;
    child.needs_layout_update = true;
    
    vec_push(&panel->children, child);
    
    panel->needs_layout_recalculation = true;
    return panel->children.length - 1;
}

void Panel_RemoveChild(Panel* panel, int index) {
    if (panel == NULL || index < 0 || index >= panel->children.length) return;
    
    vec_splice(&panel->children, index, 1);
    panel->needs_layout_recalculation = true;
}

void Panel_ClearChildren(Panel* panel) {
    if (panel == NULL) return;
    
    vec_clear(&panel->children);
    panel->needs_layout_recalculation = true;
}

int Panel_GetChildCount(const Panel* panel) {
    if (panel == NULL) return 0;
    return panel->children.length;
}

PanelChild* Panel_GetChild(Panel* panel, int index) {
    if (panel == NULL || index < 0 || index >= panel->children.length) return NULL;
    return &panel->children.data[index];
}

// Flexbox Layout Functions
void Panel_SetFlexDirection(Panel* panel, FlexDirection direction) {
    if (panel == NULL) return;
    panel->flex_direction = direction;
    panel->needs_layout_recalculation = true;
}

void Panel_SetJustifyContent(Panel* panel, JustifyContent justify) {
    if (panel == NULL) return;
    panel->justify_content = justify;
    panel->needs_layout_recalculation = true;
}

void Panel_SetAlignItems(Panel* panel, AlignItems align) {
    if (panel == NULL) return;
    panel->align_items = align;
    panel->needs_layout_recalculation = true;
}

void Panel_SetGap(Panel* panel, float gap) {
    if (panel == NULL) return;
    panel->gap = gap;
    panel->needs_layout_recalculation = true;
}

void Panel_SetChildFlexGrow(Panel* panel, int child_index, int flex_grow) {
    if (panel == NULL || child_index < 0 || child_index >= panel->children.length) return;
    panel->children.data[child_index].flex_grow = flex_grow;
    panel->needs_layout_recalculation = true;
}

void Panel_SetChildFlexShrink(Panel* panel, int child_index, int flex_shrink) {
    if (panel == NULL || child_index < 0 || child_index >= panel->children.length) return;
    panel->children.data[child_index].flex_shrink = flex_shrink;
    panel->needs_layout_recalculation = true;
}

void Panel_SetChildFlexBasis(Panel* panel, int child_index, float flex_basis) {
    if (panel == NULL || child_index < 0 || child_index >= panel->children.length) return;
    panel->children.data[child_index].flex_basis = flex_basis;
    panel->needs_layout_recalculation = true;
}

void Panel_SetChildAlignSelf(Panel* panel, int child_index, AlignItems align_self) {
    if (panel == NULL || child_index < 0 || child_index >= panel->children.length) return;
    panel->children.data[child_index].align_self = align_self;
    panel->needs_layout_recalculation = true;
}

void Panel_SetChildMargin(Panel* panel, int child_index, EdgeValues margin) {
    if (panel == NULL || child_index < 0 || child_index >= panel->children.length) return;
    panel->children.data[child_index].margin = margin;
    panel->needs_layout_recalculation = true;
}

// Layout Calculation Functions
Rectangle Panel_GetContentArea(const Panel* panel) {
    if (panel == NULL) return (Rectangle){0};
    
    // Get padding from theme
    EdgeValues padding = AppTheme.components[COMPONENT_PANEL].padding;
    
    Rectangle content_area = panel->rect;
    content_area.x += padding.left;
    content_area.y += padding.top;
    content_area.width -= (padding.left + padding.right);
    content_area.height -= (padding.top + padding.bottom);
    
    return content_area;
}

Rectangle Panel_GetChildRect(const Panel* panel, int child_index) {
    if (panel == NULL || child_index < 0 || child_index >= panel->children.length) {
        return (Rectangle){0};
    }
    
    return panel->children.data[child_index].calculated_rect;
}

void Panel_RecalculateLayout(Panel* panel) {
    if (panel == NULL || panel->children.length == 0) return;
    
    // Update content area
    panel->content_area = Panel_GetContentArea(panel);
    
    // Calculate available space for children
    float available_width = panel->content_area.width;
    float available_height = panel->content_area.height;
    
    // Calculate total gap space
    float total_gap = (panel->children.length > 1) ? panel->gap * (panel->children.length - 1) : 0.0f;
    
    // Determine main axis and cross axis dimensions
    bool is_row = (panel->flex_direction == FLEX_DIRECTION_ROW);
    float main_axis_size = is_row ? available_width : available_height;
    float cross_axis_size = is_row ? available_height : available_width;
    
    // Subtract gap space from main axis
    main_axis_size -= total_gap;
    
    // Calculate flex basis total and flex grow total
    float total_flex_basis = 0.0f;
    int total_flex_grow = 0;
    
    for (int i = 0; i < panel->children.length; i++) {
        PanelChild* child = &panel->children.data[i];
        if (child->is_visible) {
            total_flex_basis += child->flex_basis;
            total_flex_grow += child->flex_grow;
        }
    }
    
    // Calculate remaining space after flex basis
    float remaining_space = main_axis_size - total_flex_basis;
    
    // Distribute remaining space based on flex_grow
    float flex_grow_unit = (total_flex_grow > 0 && remaining_space > 0) ? remaining_space / total_flex_grow : 0.0f;
    
    // Calculate child sizes
    float* child_main_sizes = (float*)malloc(panel->children.length * sizeof(float));
    float total_main_size = 0.0f;
    
    for (int i = 0; i < panel->children.length; i++) {
        PanelChild* child = &panel->children.data[i];
        if (child->is_visible) {
            child_main_sizes[i] = child->flex_basis + (child->flex_grow * flex_grow_unit);
            total_main_size += child_main_sizes[i];
        } else {
            child_main_sizes[i] = 0.0f;
        }
    }
    
    // Calculate starting position based on justify_content
    float main_start = 0.0f;
    float main_spacing = 0.0f;
    
    switch (panel->justify_content) {
        case JUSTIFY_START:
            main_start = 0.0f;
            break;
        case JUSTIFY_CENTER:
            main_start = (main_axis_size - total_main_size) / 2.0f;
            break;
        case JUSTIFY_END:
            main_start = main_axis_size - total_main_size;
            break;
        case JUSTIFY_SPACE_BETWEEN:
            main_start = 0.0f;
            if (panel->children.length > 1) {
                main_spacing = (main_axis_size - total_main_size) / (panel->children.length - 1);
            }
            break;
        case JUSTIFY_SPACE_AROUND:
            if (panel->children.length > 0) {
                main_spacing = (main_axis_size - total_main_size) / panel->children.length;
                main_start = main_spacing / 2.0f;
            }
            break;
        case JUSTIFY_SPACE_EVENLY:
            if (panel->children.length > 0) {
                main_spacing = (main_axis_size - total_main_size) / (panel->children.length + 1);
                main_start = main_spacing;
            }
            break;
    }
    
    // Position children
    float current_main_pos = main_start;
    
    for (int i = 0; i < panel->children.length; i++) {
        PanelChild* child = &panel->children.data[i];
        if (!child->is_visible) {
            child->calculated_rect = (Rectangle){0};
            continue;
        }
        
        // Determine cross axis alignment
        AlignItems effective_align = (child->align_self != ALIGN_START) ? child->align_self : panel->align_items;
        
        float cross_pos = 0.0f;
        float cross_size = cross_axis_size;
        
        switch (effective_align) {
            case ALIGN_START:
                cross_pos = 0.0f;
                break;
            case ALIGN_CENTER:
                cross_pos = (cross_axis_size - cross_size) / 2.0f;
                break;
            case ALIGN_END:
                cross_pos = cross_axis_size - cross_size;
                break;
            case ALIGN_STRETCH:
                cross_pos = 0.0f;
                cross_size = cross_axis_size;
                break;
        }
        
        // Apply margins
        EdgeValues margin = child->margin;
        
        // Calculate final rectangle
        if (is_row) {
            child->calculated_rect.x = panel->content_area.x + current_main_pos + margin.left;
            child->calculated_rect.y = panel->content_area.y + cross_pos + margin.top;
            child->calculated_rect.width = child_main_sizes[i] - (margin.left + margin.right);
            child->calculated_rect.height = cross_size - (margin.top + margin.bottom);
        } else {
            child->calculated_rect.x = panel->content_area.x + cross_pos + margin.left;
            child->calculated_rect.y = panel->content_area.y + current_main_pos + margin.top;
            child->calculated_rect.width = cross_size - (margin.left + margin.right);
            child->calculated_rect.height = child_main_sizes[i] - (margin.top + margin.bottom);
        }
        
        // Update child component rectangle
        switch (child->type) {
            case PANEL_CHILD_BUTTON:
                if (child->component) {
                    ((Button*)child->component)->rect = child->calculated_rect;
                }
                break;
            case PANEL_CHILD_PANEL:
                if (child->component) {
                    ((Panel*)child->component)->rect = child->calculated_rect;
                    ((Panel*)child->component)->needs_layout_recalculation = true;
                }
                break;
            case PANEL_CHILD_SLIDER:
                // TODO: Update slider rect when available
                break;
            case PANEL_CHILD_SIDEBAR:
                if (child->component) {
                    Sidebar* sidebar = (Sidebar*)child->component;
                    sidebar->bounds = child->calculated_rect;
                }
                break;
            case PANEL_CHILD_CUSTOM:
                // Custom components need their own rect handling
                break;
        }
        
        // Move to next position
        current_main_pos += child_main_sizes[i] + panel->gap + main_spacing;
        child->needs_layout_update = false;
    }
    
    free(child_main_sizes);
    panel->needs_layout_recalculation = false;
}

// Visual State Functions
void Panel_SetVisible(Panel* panel, bool is_visible) {
    if (panel == NULL) return;
    panel->is_visible = is_visible;
}

void Panel_SetClipContent(Panel* panel, bool clip_content) {
    if (panel == NULL) return;
    panel->clip_content = clip_content;
}

void Panel_SetZIndex(Panel* panel, int z_index) {
    if (panel == NULL) return;
    panel->z_index = z_index;
}