#include "theme.h"

// Global theme instance
Theme AppTheme;

// Helper function to create a color palette
static ColorPalette CreateColorPalette(Color bg, Color border, Color text, Color accent, Color shadow) {
    return (ColorPalette){
        .background = bg,
        .border = border,
        .text = text,
        .accent = accent,
        .shadow = shadow
    };
}

// Helper function to initialize a component style with common defaults
static ComponentStyle CreateComponentStyle(void) {
    ComponentStyle style = {0};
    
    // Default styling that works for most components
    style.border_width = (EdgeValues){1.0f, 1.0f, 1.0f, 1.0f};
    style.outline_width = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    style.padding = (EdgeValues){8.0f, 16.0f, 8.0f, 16.0f};
    style.margin = (EdgeValues){4.0f, 4.0f, 4.0f, 4.0f};
    style.border_radius = (CornerRadius){6.0f, 6.0f, 6.0f, 6.0f};
    style.border_style = BORDER_SOLID;
    style.shadow_offset = (Vector2){2.0f, 2.0f};
    style.shadow_blur = 4.0f;
    style.shadow_spread = 0.0f;
    style.opacity = 1.0f;
    style.z_index = 1;
    style.font_size = 14;
    style.text_align = TEXT_ALIGN_CENTER;
    style.text_vertical_align = TEXT_VERTICAL_CENTER;
    style.line_height = 1.4f;
    style.letter_spacing = 0.0f;
    
    return style;
}

void Theme_Init(void) {
    Theme_Init_Default();
    Theme_InitFontManager("fonts"); // Default fonts directory
}

void Theme_Init_Default(void) {
    // Initialize base component styles
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        AppTheme.components[i] = CreateComponentStyle();
    }
    
    // BUTTON styling
    ComponentStyle* button = &AppTheme.components[COMPONENT_BUTTON];
    button->colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){52.87, 27.46, 0, 4.31},   // background - #3B82F6
        (Color){96, 165, 250, 255},   // border - #60A5FA
        (Color){255, 255, 255, 255},  // text - white
        (Color){147, 197, 253, 255},  // accent - #93C5FD
        (Color){0, 0, 0, 128}         // shadow
    );
    button->colors[STATE_HOVER] = CreateColorPalette(
        (Color){37, 99, 235, 255},    // background - #2563EB
        (Color){96, 165, 250, 255},   // border
        (Color){255, 255, 255, 255},  // text
        (Color){147, 197, 253, 255},  // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    button->colors[STATE_PRESSED] = CreateColorPalette(
        (Color){29, 78, 216, 255},    // background - #1D4ED8
        (Color){96, 165, 250, 255},   // border
        (Color){255, 255, 255, 255},  // text
        (Color){147, 197, 253, 255},  // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    button->border_radius = (CornerRadius){4.0f, 4.0f, 4.0f, 4.0f}; // Modern soft rounded corners
    button->z_index = 10;
    
    // PANEL styling
    ComponentStyle* panel = &AppTheme.components[COMPONENT_PANEL];
    panel->colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background - #1F2937
        (Color){75, 85, 99, 255},     // border - #4B5563
        (Color){209, 213, 219, 255},  // text - #D1D5DB
        (Color){107, 114, 128, 255},  // accent - #6B7280
        (Color){0, 0, 0, 128}         // shadow
    );
    panel->padding = (EdgeValues){16.0f, 16.0f, 16.0f, 16.0f};
    panel->margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    panel->border_radius = (CornerRadius){8.0f, 8.0f, 8.0f, 8.0f};
    panel->shadow_offset = (Vector2){0.0f, 4.0f};
    panel->shadow_blur = 6.0f;
    panel->shadow_spread = -1.0f;
    panel->text_align = TEXT_ALIGN_LEFT;
    
    // SIDEBAR styling
    ComponentStyle* sidebar = &AppTheme.components[COMPONENT_SIDEBAR];
    sidebar->colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){17, 24, 39, 255},     // background - #111827
        (Color){55, 65, 81, 255},     // border - #374151
        (Color){209, 213, 219, 255},  // text - #D1D5DB
        (Color){59, 130, 246, 255},   // accent - #3B82F6
        (Color){0, 0, 0, 160}         // shadow
    );
    sidebar->colors[STATE_HOVER] = CreateColorPalette(
        (Color){55, 65, 81, 255},     // background - #374151
        (Color){55, 65, 81, 255},     // border
        (Color){209, 213, 219, 255},  // text
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 160}         // shadow
    );
    sidebar->colors[STATE_ACTIVE] = CreateColorPalette(
        (Color){59, 130, 246, 255},   // background - #3B82F6
        (Color){59, 130, 246, 255},   // border
        (Color){255, 255, 255, 255},  // text - white
        (Color){96, 165, 250, 255},   // accent
        (Color){0, 0, 0, 160}         // shadow
    );
    sidebar->border_width = (EdgeValues){0.0f, 1.0f, 0.0f, 0.0f}; // Right border only
    sidebar->border_radius = (CornerRadius){0.0f, 0.0f, 0.0f, 0.0f}; // No rounding
    sidebar->padding = (EdgeValues){16.0f, 0.0f, 16.0f, 0.0f}; // Top/bottom only
    sidebar->margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    sidebar->text_align = TEXT_ALIGN_LEFT;
    sidebar->line_height = 1.2f;
    sidebar->z_index = 5;
    
    // SLIDER styling
    ComponentStyle* slider = &AppTheme.components[COMPONENT_SLIDER];
    slider->colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){75, 85, 99, 255},     // background (track) - #4B5563
        (Color){156, 163, 175, 255},  // border - #9CA3AF
        (Color){209, 213, 219, 255},  // text - #D1D5DB
        (Color){59, 130, 246, 255},   // accent (fill) - #3B82F6
        (Color){0, 0, 0, 128}         // shadow
    );
    slider->colors[STATE_HOVER] = CreateColorPalette(
        (Color){243, 244, 246, 255},  // background (handle hover) - #F3F4F6
        (Color){156, 163, 175, 255},  // border
        (Color){209, 213, 219, 255},  // text
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    slider->colors[STATE_PRESSED] = CreateColorPalette(
        (Color){229, 231, 235, 255},  // background (handle pressed) - #E5E7EB
        (Color){156, 163, 175, 255},  // border
        (Color){209, 213, 219, 255},  // text
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    slider->padding = (EdgeValues){8.0f, 0.0f, 8.0f, 0.0f}; // Vertical only
    slider->font_size = 12;
    slider->z_index = 3;
    
    // MODAL styling
    ComponentStyle* modal = &AppTheme.components[COMPONENT_MODAL];
    modal->colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background - #1F2937
        (Color){75, 85, 99, 255},     // border - #4B5563
        (Color){209, 213, 219, 255},  // text - #D1D5DB
        (Color){107, 114, 128, 255},  // accent - #6B7280
        (Color){0, 0, 0, 180}         // shadow (darker for overlay)
    );
    modal->padding = (EdgeValues){24.0f, 24.0f, 24.0f, 24.0f}; // More padding for modals
    modal->margin = (EdgeValues){0.0f, 0.0f, 0.0f, 0.0f};
    modal->border_radius = (CornerRadius){12.0f, 12.0f, 12.0f, 12.0f}; // More rounded
    modal->shadow_offset = (Vector2){0.0f, 8.0f}; // Larger shadow
    modal->shadow_blur = 25.0f;
    modal->shadow_spread = -5.0f;
    modal->text_align = TEXT_ALIGN_LEFT;
    modal->z_index = 1000; // Very high z-index to appear on top
    
    // TEXT INPUT styling
    ComponentStyle* text_input = &AppTheme.components[COMPONENT_TEXT_INPUT];
    text_input->colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background - #1F2937
        (Color){75, 85, 99, 255},     // border - #4B5563
        (Color){209, 213, 219, 255},  // text - #D1D5DB
        (Color){59, 130, 246, 255},   // accent (cursor/selection) - #3B82F6
        (Color){0, 0, 0, 80}          // shadow
    );
    text_input->colors[STATE_HOVER] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background
        (Color){96, 165, 250, 255},   // border (lighter) - #60A5FA
        (Color){209, 213, 219, 255},  // text
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 80}          // shadow
    );
    text_input->colors[STATE_ACTIVE] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background
        (Color){59, 130, 246, 255},   // border (focused) - #3B82F6
        (Color){255, 255, 255, 255},  // text (brighter when focused)
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 100}         // shadow (stronger when focused)
    );
    text_input->colors[STATE_DISABLED] = CreateColorPalette(
        (Color){17, 24, 39, 255},     // background (darker) - #111827
        (Color){55, 65, 81, 255},     // border (darker) - #374151
        (Color){107, 114, 128, 255},  // text (dimmed) - #6B7280
        (Color){75, 85, 99, 255},     // accent (dimmed)
        (Color){0, 0, 0, 40}          // shadow (weaker)
    );
    text_input->padding = (EdgeValues){12.0f, 16.0f, 12.0f, 16.0f};
    text_input->margin = (EdgeValues){4.0f, 4.0f, 4.0f, 4.0f};
    text_input->border_radius = (CornerRadius){6.0f, 6.0f, 6.0f, 6.0f};
    text_input->shadow_offset = (Vector2){0.0f, 2.0f};
    text_input->shadow_blur = 4.0f;
    text_input->shadow_spread = 0.0f;
    text_input->text_align = TEXT_ALIGN_LEFT;
    text_input->text_vertical_align = TEXT_VERTICAL_CENTER;
    text_input->font_size = 14;
    text_input->z_index = 5;
    
    // TEXT AREA styling
    ComponentStyle* text_area = &AppTheme.components[COMPONENT_TEXT_AREA];
    text_area->colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background - #1F2937
        (Color){75, 85, 99, 255},     // border - #4B5563
        (Color){209, 213, 219, 255},  // text - #D1D5DB
        (Color){59, 130, 246, 255},   // accent (cursor/selection) - #3B82F6
        (Color){0, 0, 0, 80}          // shadow
    );
    text_area->colors[STATE_HOVER] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background
        (Color){96, 165, 250, 255},   // border (lighter) - #60A5FA
        (Color){209, 213, 219, 255},  // text
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 80}          // shadow
    );
    text_area->colors[STATE_ACTIVE] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background
        (Color){59, 130, 246, 255},   // border (focused) - #3B82F6
        (Color){255, 255, 255, 255},  // text (brighter when focused)
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 100}         // shadow (stronger when focused)
    );
    text_area->colors[STATE_DISABLED] = CreateColorPalette(
        (Color){17, 24, 39, 255},     // background (darker) - #111827
        (Color){55, 65, 81, 255},     // border (darker) - #374151
        (Color){107, 114, 128, 255},  // text (dimmed) - #6B7280
        (Color){75, 85, 99, 255},     // accent (dimmed)
        (Color){0, 0, 0, 40}          // shadow (weaker)
    );
    text_area->padding = (EdgeValues){16.0f, 16.0f, 16.0f, 16.0f}; // More padding for text areas
    text_area->margin = (EdgeValues){4.0f, 4.0f, 4.0f, 4.0f};
    text_area->border_radius = (CornerRadius){8.0f, 8.0f, 8.0f, 8.0f}; // Slightly more rounded
    text_area->shadow_offset = (Vector2){0.0f, 2.0f};
    text_area->shadow_blur = 6.0f;
    text_area->shadow_spread = 0.0f;
    text_area->text_align = TEXT_ALIGN_LEFT;
    text_area->text_vertical_align = TEXT_VERTICAL_TOP; // Top align for multiline
    text_area->font_size = 14;
    text_area->z_index = 5;
    
    // CHECKBOX styling
    ComponentStyle* checkbox = &AppTheme.components[COMPONENT_CHECKBOX];
    checkbox->colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background - #1F2937
        (Color){75, 85, 99, 255},     // border - #4B5563
        (Color){209, 213, 219, 255},  // text - #D1D5DB
        (Color){59, 130, 246, 255},   // accent (check mark) - #3B82F6
        (Color){0, 0, 0, 80}          // shadow
    );
    checkbox->colors[STATE_HOVER] = CreateColorPalette(
        (Color){55, 65, 81, 255},     // background (lighter) - #374151
        (Color){96, 165, 250, 255},   // border (lighter) - #60A5FA
        (Color){209, 213, 219, 255},  // text
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 80}          // shadow
    );
    checkbox->colors[STATE_PRESSED] = CreateColorPalette(
        (Color){75, 85, 99, 255},     // background (pressed) - #4B5563
        (Color){59, 130, 246, 255},   // border (focused) - #3B82F6
        (Color){255, 255, 255, 255},  // text (brighter)
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 100}         // shadow (stronger)
    );
    checkbox->colors[STATE_ACTIVE] = CreateColorPalette(
        (Color){59, 130, 246, 255},   // background (checked) - #3B82F6
        (Color){59, 130, 246, 255},   // border
        (Color){255, 255, 255, 255},  // text
        (Color){255, 255, 255, 255},  // accent (check mark - white)
        (Color){0, 0, 0, 120}         // shadow
    );
    checkbox->colors[STATE_DISABLED] = CreateColorPalette(
        (Color){17, 24, 39, 255},     // background (darker) - #111827
        (Color){55, 65, 81, 255},     // border (darker) - #374151
        (Color){107, 114, 128, 255},  // text (dimmed) - #6B7280
        (Color){75, 85, 99, 255},     // accent (dimmed)
        (Color){0, 0, 0, 40}          // shadow (weaker)
    );
    checkbox->padding = (EdgeValues){4.0f, 8.0f, 4.0f, 8.0f};
    checkbox->margin = (EdgeValues){4.0f, 4.0f, 4.0f, 4.0f};
    checkbox->border_radius = (CornerRadius){4.0f, 4.0f, 4.0f, 4.0f};
    checkbox->shadow_offset = (Vector2){1.0f, 1.0f};
    checkbox->shadow_blur = 2.0f;
    checkbox->shadow_spread = 0.0f;
    checkbox->text_align = TEXT_ALIGN_LEFT;
    checkbox->text_vertical_align = TEXT_VERTICAL_CENTER;
    checkbox->font_size = 14;
    checkbox->z_index = 5;
    
    // RADIO BUTTON styling
    ComponentStyle* radio = &AppTheme.components[COMPONENT_RADIO_BUTTON];
    radio->colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background - #1F2937
        (Color){75, 85, 99, 255},     // border - #4B5563
        (Color){209, 213, 219, 255},  // text - #D1D5DB
        (Color){59, 130, 246, 255},   // accent (dot) - #3B82F6
        (Color){0, 0, 0, 80}          // shadow
    );
    radio->colors[STATE_HOVER] = CreateColorPalette(
        (Color){55, 65, 81, 255},     // background (lighter) - #374151
        (Color){96, 165, 250, 255},   // border (lighter) - #60A5FA
        (Color){209, 213, 219, 255},  // text
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 80}          // shadow
    );
    radio->colors[STATE_PRESSED] = CreateColorPalette(
        (Color){75, 85, 99, 255},     // background (pressed) - #4B5563
        (Color){59, 130, 246, 255},   // border (focused) - #3B82F6
        (Color){255, 255, 255, 255},  // text (brighter)
        (Color){59, 130, 246, 255},   // accent
        (Color){0, 0, 0, 100}         // shadow (stronger)
    );
    radio->colors[STATE_ACTIVE] = CreateColorPalette(
        (Color){31, 41, 55, 255},     // background (same as default)
        (Color){59, 130, 246, 255},   // border (selected) - #3B82F6
        (Color){255, 255, 255, 255},  // text
        (Color){59, 130, 246, 255},   // accent (dot) - #3B82F6
        (Color){0, 0, 0, 120}         // shadow
    );
    radio->colors[STATE_DISABLED] = CreateColorPalette(
        (Color){17, 24, 39, 255},     // background (darker) - #111827
        (Color){55, 65, 81, 255},     // border (darker) - #374151
        (Color){107, 114, 128, 255},  // text (dimmed) - #6B7280
        (Color){75, 85, 99, 255},     // accent (dimmed)
        (Color){0, 0, 0, 40}          // shadow (weaker)
    );
    radio->padding = (EdgeValues){4.0f, 8.0f, 4.0f, 8.0f};
    radio->margin = (EdgeValues){4.0f, 4.0f, 4.0f, 4.0f};
    radio->border_radius = (CornerRadius){12.0f, 12.0f, 12.0f, 12.0f}; // Circular
    radio->shadow_offset = (Vector2){1.0f, 1.0f};
    radio->shadow_blur = 2.0f;
    radio->shadow_spread = 0.0f;
    radio->text_align = TEXT_ALIGN_LEFT;
    radio->text_vertical_align = TEXT_VERTICAL_CENTER;
    radio->font_size = 14;
    radio->z_index = 5;
    
    // Button variants using the same structure
    AppTheme.primary_button = AppTheme.components[COMPONENT_BUTTON]; // Already set above
    
    // Secondary button variant
    AppTheme.secondary_button = CreateComponentStyle();
    AppTheme.secondary_button.colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){55, 65, 81, 255},     // background - #374151
        (Color){156, 163, 175, 255},  // border - #9CA3AF
        (Color){255, 255, 255, 255},  // text
        (Color){209, 213, 219, 255},  // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    AppTheme.secondary_button.colors[STATE_HOVER] = CreateColorPalette(
        (Color){75, 85, 99, 255},     // background - #4B5563
        (Color){156, 163, 175, 255},  // border
        (Color){255, 255, 255, 255},  // text
        (Color){209, 213, 219, 255},  // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    AppTheme.secondary_button.colors[STATE_PRESSED] = CreateColorPalette(
        (Color){107, 114, 128, 255},  // background - #6B7280
        (Color){156, 163, 175, 255},  // border
        (Color){255, 255, 255, 255},  // text
        (Color){209, 213, 219, 255},  // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    AppTheme.secondary_button.border_radius = (CornerRadius){4.0f, 4.0f, 4.0f, 4.0f}; // Modern soft rounded corners
    AppTheme.secondary_button.z_index = 10;
    
    // Destructive button variant
    AppTheme.destructive_button = CreateComponentStyle();
    AppTheme.destructive_button.colors[STATE_DEFAULT] = CreateColorPalette(
        (Color){220, 38, 38, 255},    // background - #DC2626
        (Color){248, 113, 113, 255},  // border - #F87171
        (Color){255, 255, 255, 255},  // text
        (Color){252, 165, 165, 255},  // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    AppTheme.destructive_button.colors[STATE_HOVER] = CreateColorPalette(
        (Color){185, 28, 28, 255},    // background - #B91C1C
        (Color){248, 113, 113, 255},  // border
        (Color){255, 255, 255, 255},  // text
        (Color){252, 165, 165, 255},  // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    AppTheme.destructive_button.colors[STATE_PRESSED] = CreateColorPalette(
        (Color){153, 27, 27, 255},    // background - #991B1B
        (Color){248, 113, 113, 255},  // border
        (Color){255, 255, 255, 255},  // text
        (Color){252, 165, 165, 255},  // accent
        (Color){0, 0, 0, 128}         // shadow
    );
    AppTheme.destructive_button.border_radius = (CornerRadius){4.0f, 4.0f, 4.0f, 4.0f}; // Modern soft rounded corners
    AppTheme.destructive_button.z_index = 10;
    
    // Component-specific properties
    AppTheme.sidebar_props = (SidebarProperties){
        .width = 240.0f,
        .item_height = 40.0f,
        .collapse_width = 60.0f,
        .auto_collapse = false
    };
    
    AppTheme.slider_props = (SliderProperties){
        .track_height = 4.0f,
        .handle_size = 16.0f,
        .min_width = 100.0f,
        .step_size = 1.0f,
        .show_value = true,
        .track_radius = 2.0f,
        .handle_radius = 8.0f
    };
    
    AppTheme.modal_props = (ModalProperties){
        .overlay_opacity = 0.7f,
        .min_width = 300.0f,
        .min_height = 200.0f,
        .max_width_percent = 0.9f,
        .max_height_percent = 0.9f,
        .close_on_overlay_click = true,
        .show_close_button = true,
        .close_button_size = 24.0f,
        .use_blur_effect = true,
        .blur_strength = 3.0f
    };
    
    AppTheme.text_input_props = (TextInputProperties){
        .cursor_width = 2.0f,
        .cursor_blink_speed = 1.0f,
        .selection_opacity = 0.3f,
        .show_placeholder = true,
        .auto_select_on_focus = false,
        .max_length = 256,
        .scroll_speed = 20.0f
    };
    
    AppTheme.text_area_props = (TextAreaProperties){
        .line_height = 1.4f,
        .scroll_bar_width = 12.0f,
        .show_line_numbers = false,
        .word_wrap = true,
        .show_scroll_bars = true,
        .scroll_speed = 30.0f,
        .tab_size = 4,
        .auto_indent = true
    };
    
    AppTheme.checkbox_props = (CheckboxProperties){
        .box_size = 18.0f,
        .label_spacing = 8.0f,
        .show_check_mark = true,
        .check_mark_thickness = 2.0f,
        .animation_speed = 8.0f
    };
    
    AppTheme.radio_button_props = (RadioButtonProperties){
        .circle_size = 18.0f,
        .dot_size = 8.0f,
        .label_spacing = 8.0f,
        .animation_speed = 8.0f
    };
    
    // Global settings
    AppTheme.transition_duration = 0.2f;
}

ComponentStyle* Theme_GetComponentStyle(ComponentType type) {
    if (type >= 0 && type < COMPONENT_COUNT) {
        return &AppTheme.components[type];
    }
    return NULL;
}

ColorPalette* Theme_GetComponentColors(ComponentType type, ComponentState state) {
    ComponentStyle* style = Theme_GetComponentStyle(type);
    if (style && state >= 0 && state < STATE_COUNT) {
        return &style->colors[state];
    }
    return NULL;
}

// Font Management Implementation

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Note: Directory scanning is simplified for cross-platform compatibility

// Helper function to check if a file has a font extension
static bool IsFontFile(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    return (strcmp(ext, ".ttf") == 0 || 
            strcmp(ext, ".TTF") == 0 ||
            strcmp(ext, ".otf") == 0 || 
            strcmp(ext, ".OTF") == 0 ||
            strcmp(ext, ".woff") == 0 || 
            strcmp(ext, ".WOFF") == 0);
}

// Helper function to extract font name from filename
static void ExtractFontName(const char* filename, char* name, size_t name_size) {
    const char* basename = strrchr(filename, '/');
    if (!basename) basename = strrchr(filename, '\\');
    if (!basename) basename = filename;
    else basename++; // Skip the slash
    
    // Copy filename without extension
    const char* ext = strrchr(basename, '.');
    size_t len = ext ? (size_t)(ext - basename) : strlen(basename);
    len = len < name_size - 1 ? len : name_size - 1;
    
    strncpy(name, basename, len);
    name[len] = '\0';
    
    // Replace underscores and hyphens with spaces
    for (size_t i = 0; i < len; i++) {
        if (name[i] == '_' || name[i] == '-') {
            name[i] = ' ';
        }
    }
}

void Theme_InitFontManager(const char* fonts_directory) {
    FontManager* fm = &AppTheme.font_manager;
    
    // Initialize font manager
    fm->fonts = NULL;
    fm->font_count = 0;
    fm->current_font_index = -1;
    fm->auto_scan_fonts = true;
    
    // Set fonts directory
    if (fonts_directory) {
        strncpy(fm->fonts_directory, fonts_directory, sizeof(fm->fonts_directory) - 1);
        fm->fonts_directory[sizeof(fm->fonts_directory) - 1] = '\0';
    } else {
        strcpy(fm->fonts_directory, "fonts");
    }
    
    // Add default system font
    fm->fonts = (ThemeFont*)malloc(sizeof(ThemeFont));
    if (fm->fonts) {
        ThemeFont* default_font = &fm->fonts[0];
        strcpy(default_font->name, "Default System Font");
        strcpy(default_font->filename, "");
        strcpy(default_font->filepath, "");
        default_font->font = GetFontDefault();
        default_font->is_loaded = true;
        default_font->is_default = true;
        
        fm->font_count = 1;
        fm->current_font_index = 0;
    }
    
    // Scan for additional fonts
    if (fm->auto_scan_fonts) {
        Theme_ScanFonts();
    }
}

void Theme_ScanFonts(void) {
    FontManager* fm = &AppTheme.font_manager;
    
    // For now, we'll add some common font examples manually
    // In a full implementation, this would scan the fonts directory
    
    printf("Scanning fonts directory: %s\n", fm->fonts_directory);
    
    // Add some example fonts that users can place in the fonts folder
    const char* example_fonts[] = {
        "Roboto-Regular.ttf",
        "OpenSans-Regular.ttf", 
        "Lato-Regular.ttf",
        "Montserrat-Regular.ttf",
        "SourceSansPro-Regular.ttf"
    };
    
    for (int i = 0; i < 5; i++) {
        // Check if font file exists
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", fm->fonts_directory, example_fonts[i]);
        
        FILE* test_file = fopen(filepath, "rb");
        if (test_file) {
            fclose(test_file);
            
            // Check if font already exists
            bool already_exists = false;
            for (int j = 0; j < fm->font_count; j++) {
                if (strcmp(fm->fonts[j].filename, example_fonts[i]) == 0) {
                    already_exists = true;
                    break;
                }
            }
            
            if (!already_exists) {
                // Add new font
                ThemeFont* new_fonts = (ThemeFont*)realloc(fm->fonts, (fm->font_count + 1) * sizeof(ThemeFont));
                if (new_fonts) {
                    fm->fonts = new_fonts;
                    ThemeFont* new_font = &fm->fonts[fm->font_count];
                    
                    // Extract font name from filename
                    ExtractFontName(example_fonts[i], new_font->name, sizeof(new_font->name));
                    
                    // Set filename and filepath
                    strncpy(new_font->filename, example_fonts[i], sizeof(new_font->filename) - 1);
                    new_font->filename[sizeof(new_font->filename) - 1] = '\0';
                    
                    strncpy(new_font->filepath, filepath, sizeof(new_font->filepath) - 1);
                    new_font->filepath[sizeof(new_font->filepath) - 1] = '\0';
                    
                    // Initialize font properties
                    new_font->font = (Font){0};
                    new_font->is_loaded = false;
                    new_font->is_default = false;
                    
                    fm->font_count++;
                    
                    printf("Found font: %s (%s)\n", new_font->name, new_font->filename);
                }
            }
        }
    }
    
    printf("Font scanning complete. Found %d fonts total.\n", fm->font_count);
}

bool Theme_LoadFont(int font_index, int font_size) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (font_index < 0 || font_index >= fm->font_count) {
        return false;
    }
    
    ThemeFont* font = &fm->fonts[font_index];
    
    // Don't reload default font
    if (font->is_default) {
        return true;
    }
    
    // Unload existing font if loaded
    if (font->is_loaded) {
        UnloadFont(font->font);
        font->is_loaded = false;
    }
    
    // Load new font
    font->font = LoadFontEx(font->filepath, font_size, NULL, 0);
    
    if (font->font.texture.id > 0) {
        font->is_loaded = true;
        printf("Loaded font: %s at size %d\n", font->name, font_size);
        return true;
    } else {
        printf("Failed to load font: %s\n", font->filepath);
        return false;
    }
}

void Theme_UnloadFont(int font_index) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (font_index < 0 || font_index >= fm->font_count) {
        return;
    }
    
    ThemeFont* font = &fm->fonts[font_index];
    
    // Don't unload default font
    if (font->is_default) {
        return;
    }
    
    if (font->is_loaded) {
        UnloadFont(font->font);
        font->is_loaded = false;
        font->font = (Font){0};
    }
}

void Theme_SetCurrentFont(int font_index) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (font_index < 0 || font_index >= fm->font_count) {
        return;
    }
    
    // Load font if not already loaded
    if (!fm->fonts[font_index].is_loaded && !fm->fonts[font_index].is_default) {
        if (!Theme_LoadFont(font_index, 16)) { // Default size
            return;
        }
    }
    
    fm->current_font_index = font_index;
    printf("Set current font to: %s\n", fm->fonts[font_index].name);
}

Font Theme_GetCurrentFont(void) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (fm->current_font_index >= 0 && fm->current_font_index < fm->font_count) {
        return fm->fonts[fm->current_font_index].font;
    }
    
    return GetFontDefault();
}

Font Theme_GetFont(int font_index) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (font_index >= 0 && font_index < fm->font_count && fm->fonts[font_index].is_loaded) {
        return fm->fonts[font_index].font;
    }
    
    return GetFontDefault();
}

const char* Theme_GetFontName(int font_index) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (font_index >= 0 && font_index < fm->font_count) {
        return fm->fonts[font_index].name;
    }
    
    return "Unknown Font";
}

int Theme_GetFontCount(void) {
    return AppTheme.font_manager.font_count;
}

int Theme_GetCurrentFontIndex(void) {
    return AppTheme.font_manager.current_font_index;
}

int Theme_FindFontByName(const char* font_name) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (!font_name) return -1;
    
    for (int i = 0; i < fm->font_count; i++) {
        if (strcmp(fm->fonts[i].name, font_name) == 0) {
            return i;
        }
    }
    
    return -1;
}

bool Theme_AddCustomFont(const char* name, const char* filepath) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (!name || !filepath) return false;
    
    // Check if font already exists
    for (int i = 0; i < fm->font_count; i++) {
        if (strcmp(fm->fonts[i].filepath, filepath) == 0) {
            return false; // Already exists
        }
    }
    
    // Add new font
    ThemeFont* new_fonts = (ThemeFont*)realloc(fm->fonts, (fm->font_count + 1) * sizeof(ThemeFont));
    if (!new_fonts) return false;
    
    fm->fonts = new_fonts;
    ThemeFont* new_font = &fm->fonts[fm->font_count];
    
    // Set font properties
    strncpy(new_font->name, name, sizeof(new_font->name) - 1);
    new_font->name[sizeof(new_font->name) - 1] = '\0';
    
    strncpy(new_font->filepath, filepath, sizeof(new_font->filepath) - 1);
    new_font->filepath[sizeof(new_font->filepath) - 1] = '\0';
    
    // Extract filename from filepath
    const char* filename = strrchr(filepath, '/');
    if (!filename) filename = strrchr(filepath, '\\');
    if (!filename) filename = filepath;
    else filename++; // Skip the slash
    
    strncpy(new_font->filename, filename, sizeof(new_font->filename) - 1);
    new_font->filename[sizeof(new_font->filename) - 1] = '\0';
    
    // Initialize font properties
    new_font->font = (Font){0};
    new_font->is_loaded = false;
    new_font->is_default = false;
    
    fm->font_count++;
    
    return true;
}

void Theme_CleanupFonts(void) {
    FontManager* fm = &AppTheme.font_manager;
    
    // Unload all fonts except default
    for (int i = 0; i < fm->font_count; i++) {
        if (!fm->fonts[i].is_default && fm->fonts[i].is_loaded) {
            UnloadFont(fm->fonts[i].font);
        }
    }
    
    // Free fonts array
    if (fm->fonts) {
        free(fm->fonts);
        fm->fonts = NULL;
    }
    
    fm->font_count = 0;
    fm->current_font_index = -1;
}

// Font utility functions
bool Theme_IsFontLoaded(int font_index) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (font_index >= 0 && font_index < fm->font_count) {
        return fm->fonts[font_index].is_loaded;
    }
    
    return false;
}

void Theme_ReloadAllFonts(int font_size) {
    FontManager* fm = &AppTheme.font_manager;
    
    for (int i = 0; i < fm->font_count; i++) {
        if (!fm->fonts[i].is_default) {
            Theme_LoadFont(i, font_size);
        }
    }
}

void Theme_SetFontsDirectory(const char* directory) {
    FontManager* fm = &AppTheme.font_manager;
    
    if (directory) {
        strncpy(fm->fonts_directory, directory, sizeof(fm->fonts_directory) - 1);
        fm->fonts_directory[sizeof(fm->fonts_directory) - 1] = '\0';
        
        if (fm->auto_scan_fonts) {
            Theme_ScanFonts();
        }
    }
}

const char* Theme_GetFontsDirectory(void) {
    return AppTheme.font_manager.fonts_directory;
}