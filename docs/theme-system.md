# Theme System Guide

The theme system is the heart of the UI library, providing consistent styling across all components while allowing for easy customization.

## Architecture Overview

The theme system uses a scalable, component-agnostic approach:

```c
// Generic styling that works for any component
typedef struct {
    ColorPalette colors[STATE_COUNT];  // Colors for different states
    EdgeValues border_width;           // Border thickness
    EdgeValues padding;                // Internal spacing
    EdgeValues margin;                 // External spacing
    CornerRadius border_radius;        // Rounded corners
    Vector2 shadow_offset;             // Shadow position
    float shadow_blur;                 // Shadow softness
    // ... more properties
} ComponentStyle;

// Theme contains styles for all components
typedef struct {
    ComponentStyle components[COMPONENT_COUNT];  // Base styles
    ComponentStyle primary_button;               // Button variants
    ComponentStyle secondary_button;
    ComponentStyle destructive_button;
    // Component-specific properties
    SidebarProperties sidebar_props;
    SliderProperties slider_props;
    ModalProperties modal_props;
} Theme;
```

## Using the Theme System

### Basic Usage

```c
// Initialize with default dark theme
Theme_Init_Default();

// Access component styles
ComponentStyle* button_style = Theme_GetComponentStyle(COMPONENT_BUTTON);
ColorPalette* hover_colors = Theme_GetComponentColors(COMPONENT_BUTTON, STATE_HOVER);
```

### Customizing Colors

```c
// Change button colors
ComponentStyle* button = &AppTheme.components[COMPONENT_BUTTON];
button->colors[STATE_DEFAULT].background = BLUE;
button->colors[STATE_HOVER].background = DARKBLUE;
button->colors[STATE_PRESSED].background = NAVY;

// Change text colors
button->colors[STATE_DEFAULT].text = WHITE;
button->colors[STATE_HOVER].text = LIGHTGRAY;
```

### Customizing Layout Properties

```c
// Change button padding
AppTheme.components[COMPONENT_BUTTON].padding = (EdgeValues){12, 24, 12, 24};

// Change border radius
AppTheme.components[COMPONENT_BUTTON].border_radius = (CornerRadius){8, 8, 8, 8};

// Change shadows
AppTheme.components[COMPONENT_BUTTON].shadow_offset = (Vector2){4, 4};
AppTheme.components[COMPONENT_BUTTON].shadow_blur = 8.0f;
```

## Component States

Each component can have different styling for different states:

```c
typedef enum {
    STATE_DEFAULT,   // Normal state
    STATE_HOVER,     // Mouse over
    STATE_PRESSED,   // Being clicked
    STATE_ACTIVE,    // Selected/active
    STATE_DISABLED,  // Not interactive
    STATE_COUNT
} ComponentState;
```

### Example: Hover Effects

```c
ComponentStyle* button = &AppTheme.components[COMPONENT_BUTTON];

// Default state
button->colors[STATE_DEFAULT] = (ColorPalette){
    .background = BLUE,
    .border = DARKBLUE,
    .text = WHITE,
    .accent = LIGHTBLUE,
    .shadow = (Color){0, 0, 0, 128}
};

// Hover state - slightly different
button->colors[STATE_HOVER] = (ColorPalette){
    .background = DARKBLUE,    // Darker background
    .border = NAVY,            // Darker border
    .text = WHITE,             // Same text
    .accent = LIGHTBLUE,       // Same accent
    .shadow = (Color){0, 0, 0, 160}  // Stronger shadow
};
```

## Button Variants

The theme system supports multiple button variants:

```c
// Primary button (default)
Button primary = Button_Create(bounds, "Primary", BUTTON_PRIMARY);

// Secondary button (uses AppTheme.secondary_button style)
Button secondary = Button_Create(bounds, "Secondary", BUTTON_SECONDARY);

// Destructive button (uses AppTheme.destructive_button style)
Button destructive = Button_Create(bounds, "Delete", BUTTON_DESTRUCTIVE);
```

### Customizing Button Variants

```c
// Customize secondary button
AppTheme.secondary_button.colors[STATE_DEFAULT] = (ColorPalette){
    .background = GRAY,
    .border = DARKGRAY,
    .text = WHITE,
    .accent = LIGHTGRAY,
    .shadow = (Color){0, 0, 0, 100}
};

// Customize destructive button
AppTheme.destructive_button.colors[STATE_DEFAULT] = (ColorPalette){
    .background = RED,
    .border = DARKRED,
    .text = WHITE,
    .accent = PINK,
    .shadow = (Color){0, 0, 0, 150}
};
```

## Component-Specific Properties

Some components have unique properties that don't fit the generic model:

### Sidebar Properties
```c
AppTheme.sidebar_props = (SidebarProperties){
    .width = 280.0f,           // Default width
    .item_height = 48.0f,      // Height of each item
    .collapse_width = 64.0f,   // Width when collapsed
    .auto_collapse = false     // Auto-collapse behavior
};
```

### Slider Properties
```c
AppTheme.slider_props = (SliderProperties){
    .track_height = 6.0f,      // Track thickness
    .handle_size = 20.0f,      // Handle diameter
    .min_width = 120.0f,       // Minimum slider width
    .step_size = 1.0f,         // Value increment
    .show_value = true,        // Show value text
    .track_radius = 3.0f,      // Track corner radius
    .handle_radius = 10.0f     // Handle corner radius
};
```

### Modal Properties
```c
AppTheme.modal_props = (ModalProperties){
    .overlay_opacity = 0.7f,           // Background overlay
    .min_width = 320.0f,               // Minimum modal width
    .min_height = 240.0f,              // Minimum modal height
    .max_width_percent = 0.9f,         // Max width as % of screen
    .max_height_percent = 0.9f,        // Max height as % of screen
    .close_on_overlay_click = true,    // Click outside to close
    .show_close_button = true,         // Show X button
    .close_button_size = 28.0f,        // Close button size
    .use_blur_effect = true,           // Enable backdrop blur
    .blur_strength = 4.0f              // Blur intensity
};
```

## Creating Custom Themes

### Light Theme Example

```c
void Theme_Init_Light(void) {
    // Start with defaults
    Theme_Init_Default();
    
    // Override with light colors
    ComponentStyle* button = &AppTheme.components[COMPONENT_BUTTON];
    button->colors[STATE_DEFAULT] = (ColorPalette){
        .background = WHITE,
        .border = LIGHTGRAY,
        .text = BLACK,
        .accent = BLUE,
        .shadow = (Color){0, 0, 0, 50}
    };
    
    ComponentStyle* panel = &AppTheme.components[COMPONENT_PANEL];
    panel->colors[STATE_DEFAULT] = (ColorPalette){
        .background = (Color){248, 249, 250, 255},  // Very light gray
        .border = (Color){229, 231, 235, 255},      // Light gray border
        .text = (Color){17, 24, 39, 255},           // Dark text
        .accent = BLUE,
        .shadow = (Color){0, 0, 0, 30}
    };
    
    // Update all components...
}
```

### High Contrast Theme

```c
void Theme_Init_HighContrast(void) {
    Theme_Init_Default();
    
    // High contrast colors for accessibility
    ComponentStyle* button = &AppTheme.components[COMPONENT_BUTTON];
    button->colors[STATE_DEFAULT] = (ColorPalette){
        .background = BLACK,
        .border = WHITE,
        .text = WHITE,
        .accent = YELLOW,
        .shadow = (Color){255, 255, 255, 100}  // White shadow
    };
    
    button->colors[STATE_HOVER] = (ColorPalette){
        .background = WHITE,
        .border = BLACK,
        .text = BLACK,
        .accent = BLUE,
        .shadow = (Color){0, 0, 0, 100}
    };
}
```

## Runtime Theme Switching

```c
typedef enum {
    THEME_DARK,
    THEME_LIGHT,
    THEME_HIGH_CONTRAST
} ThemeType;

void SetTheme(ThemeType theme) {
    switch (theme) {
        case THEME_DARK:
            Theme_Init_Default();
            break;
        case THEME_LIGHT:
            Theme_Init_Light();
            break;
        case THEME_HIGH_CONTRAST:
            Theme_Init_HighContrast();
            break;
    }
}

// Usage
SetTheme(THEME_LIGHT);  // Switch to light theme
```

## Best Practices

### 1. Consistent Color Palettes
Use a consistent color system across your theme:

```c
// Define your color palette
#define PRIMARY_COLOR    (Color){59, 130, 246, 255}   // Blue
#define PRIMARY_HOVER    (Color){37, 99, 235, 255}    // Darker blue
#define PRIMARY_PRESSED  (Color){29, 78, 216, 255}    // Even darker
#define SECONDARY_COLOR  (Color){107, 114, 128, 255}  // Gray
#define SUCCESS_COLOR    (Color){34, 197, 94, 255}    // Green
#define DANGER_COLOR     (Color){239, 68, 68, 255}    // Red
```

### 2. Semantic Naming
Create semantic theme functions:

```c
void Theme_SetPrimaryColor(Color color) {
    AppTheme.primary_button.colors[STATE_DEFAULT].background = color;
    AppTheme.components[COMPONENT_BUTTON].colors[STATE_DEFAULT].accent = color;
    // Update other components that use primary color
}

void Theme_SetDangerColor(Color color) {
    AppTheme.destructive_button.colors[STATE_DEFAULT].background = color;
}
```

### 3. Responsive Sizing
Make your theme responsive to screen size:

```c
void Theme_UpdateForScreenSize(int width, int height) {
    if (width < 768) {  // Mobile
        AppTheme.components[COMPONENT_BUTTON].padding = (EdgeValues){6, 12, 6, 12};
        AppTheme.components[COMPONENT_BUTTON].font_size = 14;
        AppTheme.sidebar_props.width = width * 0.8f;
    } else {  // Desktop
        AppTheme.components[COMPONENT_BUTTON].padding = (EdgeValues){8, 16, 8, 16};
        AppTheme.components[COMPONENT_BUTTON].font_size = 16;
        AppTheme.sidebar_props.width = 280.0f;
    }
}
```

### 4. Animation-Friendly Themes
Design themes with animations in mind:

```c
// Smooth transitions between states
AppTheme.transition_duration = 0.2f;  // 200ms transitions

// Subtle differences between states for smooth animations
button->colors[STATE_HOVER].background = ColorLerp(
    button->colors[STATE_DEFAULT].background,
    BLACK,
    0.1f  // 10% darker
);
```

## Advanced Customization

### Per-Component Overrides
You can override theme properties for individual components:

```c
Button special_button = Button_Create(bounds, "Special", BUTTON_PRIMARY);

// Override just this button's colors
Button_SetBackgroundColor(&special_button, GOLD);
Button_SetTextColor(&special_button, BLACK);
Button_SetPadding(&special_button, 16, 32, 16, 32);
```

### Dynamic Theme Properties
Create themes that respond to application state:

```c
void UpdateThemeForGameState(GameState state) {
    switch (state) {
        case GAME_MENU:
            AppTheme.components[COMPONENT_BUTTON].shadow_blur = 8.0f;
            break;
        case GAME_PLAYING:
            AppTheme.components[COMPONENT_BUTTON].shadow_blur = 2.0f;  // Subtle
            break;
        case GAME_PAUSED:
            // Desaturate colors
            for (int i = 0; i < COMPONENT_COUNT; i++) {
                ComponentStyle* style = &AppTheme.components[i];
                style->opacity = 0.7f;
            }
            break;
    }
}
```

This theme system provides the foundation for creating beautiful, consistent, and customizable user interfaces while remaining flexible enough for any design requirements.