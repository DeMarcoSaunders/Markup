# Visual Effects Guide

The UI library includes advanced visual effects to create modern, polished interfaces. This guide covers shadows, blur effects, and animations.

## Shadow System

The shadow system provides multiple shadow types for different visual needs.

### Shadow Types

```c
typedef enum {
    SHADOW_TYPE_SIMPLE,     // Fast colored rectangle shadow
    SHADOW_TYPE_SOFT,       // Blurred shadow using blur effect
    SHADOW_TYPE_INSET,      // Inner shadow effect
    SHADOW_TYPE_GLOW        // Glow effect around components
} ShadowType;
```

### Basic Shadow Usage

```c
#include "shadow.h"

// Simple shadow
ShadowConfig simple = Shadow_Create(
    SHADOW_TYPE_SIMPLE,
    (Vector2){3, 3},        // offset
    0,                      // blur (not used for simple)
    0,                      // spread
    (Color){0, 0, 0, 100}   // color
);

Rectangle button_bounds = {100, 100, 200, 50};
Shadow_DrawRectangle(button_bounds, &simple);
DrawRectangle(100, 100, 200, 50, BLUE);  // Draw button on top
```

### Soft Blurred Shadows

```c
// Soft shadow with blur
ShadowConfig soft = Shadow_Create(
    SHADOW_TYPE_SOFT,
    (Vector2){5, 5},        // offset
    8.0f,                   // blur radius
    2.0f,                   // spread
    (Color){0, 0, 0, 150}   // color
);

Shadow_DrawRectangleRounded(button_bounds, 8.0f, &soft);
DrawRectangleRounded(button_bounds, 0.2f, 8, GREEN);
```

### Theme-Based Shadows

```c
// Automatically use theme shadow settings
Shadow_DrawComponentShadow(button_bounds, COMPONENT_BUTTON, STATE_HOVER);
DrawRectangleRounded(button_bounds, 0.2f, 8, RED);
```

### Glow Effects

```c
// Glow effect (no offset, just blur)
ShadowConfig glow = Shadow_Create(
    SHADOW_TYPE_GLOW,
    (Vector2){0, 0},        // no offset
    12.0f,                  // blur radius
    5.0f,                   // spread
    (Color){100, 200, 255, 100}  // blue glow
);

Shadow_DrawCircle((Vector2){400, 300}, 50, &glow);
DrawCircle(400, 300, 50, WHITE);
```

### Advanced Shadow Techniques

#### Layered Shadows
```c
// Multiple shadows for depth
ShadowConfig shadow1 = Shadow_Create(SHADOW_TYPE_SOFT, (Vector2){2, 2}, 4.0f, 0, (Color){0, 0, 0, 80});
ShadowConfig shadow2 = Shadow_Create(SHADOW_TYPE_SOFT, (Vector2){8, 8}, 16.0f, -2.0f, (Color){0, 0, 0, 40});

Shadow_DrawRectangle(bounds, &shadow2);  // Large, soft shadow
Shadow_DrawRectangle(bounds, &shadow1);  // Small, sharp shadow
DrawRectangle(bounds.x, bounds.y, bounds.width, bounds.height, BLUE);
```

#### Animated Shadows
```c
// Shadow that changes with interaction
void DrawAnimatedButton(Rectangle bounds, bool is_hovered, float animation_progress) {
    ShadowConfig shadow = Shadow_Create(
        SHADOW_TYPE_SOFT,
        (Vector2){2 + animation_progress * 4, 2 + animation_progress * 4},
        4.0f + animation_progress * 8.0f,
        0,
        (Color){0, 0, 0, (unsigned char)(100 + animation_progress * 50)}
    );
    
    Shadow_DrawRectangle(bounds, &shadow);
    DrawRectangle(bounds.x, bounds.y, bounds.width, bounds.height, 
                  is_hovered ? DARKBLUE : BLUE);
}
```

## Blur Effects

The blur system provides CSS-like backdrop blur effects.

### Basic Blur Setup

```c
#include "blur_effect.h"

// Initialize blur system (call once at startup)
BlurEffect_Init();

// Cleanup (call before exit)
BlurEffect_Destroy();
```

### Simple Blur Usage

```c
// Capture background content
BlurEffect_BeginCapture();
DrawBackground();  // Draw your background content
BlurEffect_EndCapture();

// Apply blur
BlurEffect_ApplyBlur(5.0f);  // blur strength

// Draw blurred result
BlurEffect_DrawBlurred();

// Draw foreground content on top
DrawForeground();
```

### Backdrop System

The backdrop system provides high-level blur effects for common use cases:

```c
#include "backdrop.h"

// Simple overlay
BackdropConfig overlay = Backdrop_CreateOverlay((Color){0, 0, 0, 128});

// Blur effect
BackdropConfig blur = Backdrop_CreateBlur(6.0f);

// Blur with overlay
BackdropConfig blur_overlay = Backdrop_CreateBlurOverlay(4.0f, (Color){0, 0, 0, 80});

// Draw backdrop
void draw_background() {
    // Draw your UI here
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), DARKBLUE);
    DrawText("Background Content", 100, 100, 20, WHITE);
}

Backdrop_DrawAnimated(&blur_overlay, draw_background, animation_progress);
```

### Modal Blur Effects

```c
// Modal with automatic blur backdrop
Modal dialog = Modal_Create("Settings", MODAL_SIZE_MEDIUM);

// Enable blur (enabled by default in theme)
Modal_SetBlurEffect(&dialog, true);

// Draw with background blur
void draw_app_background() {
    // Draw your main app UI
    DrawMainInterface();
}

// In main loop
Modal_DrawWithBackground(&dialog, draw_app_background);
```

### Custom Blur Effects

```c
// Blur a specific texture
Texture2D my_texture = LoadTexture("image.png");
Rectangle source = {0, 0, my_texture.width, my_texture.height};
Rectangle dest = {100, 100, 200, 200};

BlurEffect_DrawTextureBlurred(my_texture, source, dest, 3.0f, WHITE);
```

## Animations

The library supports smooth animations through the theme system and component states.

### Theme-Based Transitions

```c
// Set global transition duration
AppTheme.transition_duration = 0.3f;  // 300ms transitions

// Components will automatically animate between states
Button button = Button_Create(bounds, "Hover Me", BUTTON_PRIMARY);
// Hover effects will smoothly transition over 300ms
```

### Manual Animation Control

```c
typedef struct {
    float progress;
    float target;
    float speed;
} Animation;

void UpdateAnimation(Animation* anim, float delta_time) {
    if (anim->progress < anim->target) {
        anim->progress += anim->speed * delta_time;
        if (anim->progress > anim->target) anim->progress = anim->target;
    } else if (anim->progress > anim->target) {
        anim->progress -= anim->speed * delta_time;
        if (anim->progress < anim->target) anim->progress = anim->target;
    }
}

// Usage
Animation button_hover = {0.0f, 0.0f, 5.0f};  // progress, target, speed

// In update loop
if (button_is_hovered) {
    button_hover.target = 1.0f;
} else {
    button_hover.target = 0.0f;
}
UpdateAnimation(&button_hover, GetFrameTime());

// In draw loop
Color button_color = ColorLerp(BLUE, DARKBLUE, button_hover.progress);
DrawRectangle(bounds.x, bounds.y, bounds.width, bounds.height, button_color);
```

### Modal Animations

Modals have built-in animation support:

```c
Modal dialog = Modal_Create("Animated Dialog", MODAL_SIZE_MEDIUM);

// Show with animation
Modal_Show(&dialog);  // Automatically animates in

// Hide with animation
Modal_Hide(&dialog);  // Automatically animates out

// Check animation state
if (Modal_IsVisible(&dialog)) {
    // Modal is visible or animating
}
```

### Custom Component Animations

```c
typedef struct {
    Rectangle bounds;
    Animation scale_anim;
    Animation opacity_anim;
    bool is_hovered;
} AnimatedButton;

void AnimatedButton_Update(AnimatedButton* btn) {
    // Update hover state
    Vector2 mouse = GetMousePosition();
    btn->is_hovered = CheckCollisionPointRec(mouse, btn->bounds);
    
    // Update animations
    btn->scale_anim.target = btn->is_hovered ? 1.1f : 1.0f;
    btn->opacity_anim.target = btn->is_hovered ? 1.0f : 0.8f;
    
    UpdateAnimation(&btn->scale_anim, GetFrameTime());
    UpdateAnimation(&btn->opacity_anim, GetFrameTime());
}

void AnimatedButton_Draw(AnimatedButton* btn) {
    // Calculate animated properties
    float scale = btn->scale_anim.progress;
    float opacity = btn->opacity_anim.progress;
    
    Rectangle scaled_bounds = {
        btn->bounds.x - (btn->bounds.width * (scale - 1.0f)) * 0.5f,
        btn->bounds.y - (btn->bounds.height * (scale - 1.0f)) * 0.5f,
        btn->bounds.width * scale,
        btn->bounds.height * scale
    };
    
    Color button_color = BLUE;
    button_color.a = (unsigned char)(255 * opacity);
    
    DrawRectangle(scaled_bounds.x, scaled_bounds.y, 
                  scaled_bounds.width, scaled_bounds.height, button_color);
}
```

## Performance Considerations

### Shadow Performance

```c
// Use simple shadows for many components
ShadowConfig simple = Shadow_Create(SHADOW_TYPE_SIMPLE, offset, 0, 0, color);

// Use soft shadows sparingly for important elements
ShadowConfig soft = Shadow_Create(SHADOW_TYPE_SOFT, offset, blur, spread, color);

// Cache shadow textures for repeated use
static RenderTexture2D cached_shadow = {0};
if (cached_shadow.id == 0) {
    // Create shadow texture once
    cached_shadow = CreateShadowTexture();
}
```

### Blur Performance

```c
// Initialize blur system once
BlurEffect_Init();

// Resize when window changes
if (window_resized) {
    BlurEffect_Resize(new_width, new_height);
}

// Use blur sparingly - it's expensive
if (modal_is_visible && blur_enabled) {
    BlurEffect_ApplyBlur(strength);
}
```

### Animation Performance

```c
// Limit animation updates
static float last_update = 0;
float current_time = GetTime();
if (current_time - last_update > 1.0f/60.0f) {  // 60 FPS max
    UpdateAnimations();
    last_update = current_time;
}

// Use easing functions for smooth animations
float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

float eased_progress = EaseOutCubic(linear_progress);
```

## Best Practices

### 1. Consistent Shadow Hierarchy
```c
// Define shadow levels for different UI layers
#define SHADOW_LEVEL_1  Shadow_Create(SHADOW_TYPE_SIMPLE, (Vector2){1,1}, 0, 0, (Color){0,0,0,50})
#define SHADOW_LEVEL_2  Shadow_Create(SHADOW_TYPE_SOFT, (Vector2){2,2}, 4, 0, (Color){0,0,0,80})
#define SHADOW_LEVEL_3  Shadow_Create(SHADOW_TYPE_SOFT, (Vector2){4,4}, 8, 0, (Color){0,0,0,100})
#define SHADOW_LEVEL_4  Shadow_Create(SHADOW_TYPE_SOFT, (Vector2){8,8}, 16, 0, (Color){0,0,0,120})

// Use consistently
Shadow_DrawRectangle(button_bounds, &SHADOW_LEVEL_1);      // Buttons
Shadow_DrawRectangle(panel_bounds, &SHADOW_LEVEL_2);       // Panels
Shadow_DrawRectangle(modal_bounds, &SHADOW_LEVEL_4);       // Modals
```

### 2. Subtle Animations
```c
// Prefer subtle, fast animations
AppTheme.transition_duration = 0.15f;  // 150ms - feels responsive

// Use appropriate easing
float EaseOutQuart(float t) {
    return 1.0f - powf(1.0f - t, 4.0f);  // Quick start, slow end
}
```

### 3. Accessibility Considerations
```c
// Respect user preferences
bool reduce_motion = GetUserPrefersReducedMotion();  // Hypothetical function

if (reduce_motion) {
    AppTheme.transition_duration = 0.0f;  // Disable animations
    // Use simple shadows instead of soft
    shadow_config.type = SHADOW_TYPE_SIMPLE;
}
```

### 4. Progressive Enhancement
```c
// Check capabilities before using advanced effects
if (BlurEffect_IsReady()) {
    // Use blur effects
    Modal_SetBlurEffect(&modal, true);
} else {
    // Fallback to simple overlay
    Modal_SetBlurEffect(&modal, false);
}

// Graceful degradation for shadows
if (GetFPS() < 30) {
    // Switch to simple shadows for performance
    for (int i = 0; i < component_count; i++) {
        shadow_configs[i].type = SHADOW_TYPE_SIMPLE;
    }
}
```

This visual effects system provides the tools to create modern, polished interfaces while maintaining good performance and accessibility.