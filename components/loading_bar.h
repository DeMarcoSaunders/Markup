#ifndef LOADING_BAR_H
#define LOADING_BAR_H

#include "raylib.h"
#include "component_base.h"
#include <stdbool.h>

// Loading bar styles
typedef enum {
    LOADING_BAR_STYLE_LINEAR,
    LOADING_BAR_STYLE_CIRCULAR,
    LOADING_BAR_STYLE_PULSING,
    LOADING_BAR_STYLE_GRADIENT,
    LOADING_BAR_STYLE_SEGMENTED
} LoadingBarStyle;

// Loading bar directions
typedef enum {
    LOADING_BAR_DIRECTION_LEFT_TO_RIGHT,
    LOADING_BAR_DIRECTION_RIGHT_TO_LEFT,
    LOADING_BAR_DIRECTION_TOP_TO_BOTTOM,
    LOADING_BAR_DIRECTION_BOTTOM_TO_TOP
} LoadingBarDirection;

// Loading bar animation types
typedef enum {
    LOADING_BAR_ANIMATION_NONE,
    LOADING_BAR_ANIMATION_SMOOTH,
    LOADING_BAR_ANIMATION_BOUNCE,
    LOADING_BAR_ANIMATION_PULSE,
    LOADING_BAR_ANIMATION_WAVE
} LoadingBarAnimation;

typedef struct {
    // Base component
    ComponentBase base;
    
    // Progress values
    float progress;           // Current progress (0.0 to 1.0)
    float target_progress;    // Target progress for smooth animation
    float min_value;          // Minimum value
    float max_value;          // Maximum value
    float current_value;      // Current value
    
    // Visual properties
    LoadingBarStyle style;
    LoadingBarDirection direction;
    LoadingBarAnimation animation;
    
    // Colors
    Color background_color;
    Color progress_color;
    Color border_color;
    Color text_color;
    
    // Dimensions
    float bar_thickness;      // Thickness of the progress bar
    float border_width;       // Border width
    float corner_radius;      // Corner radius for rounded bars
    
    // Animation properties
    float animation_speed;    // Speed of smooth animation
    float pulse_scale;        // Current pulse scale
    float wave_offset;        // Wave animation offset
    float bounce_height;      // Bounce animation height
    
    // Text display
    bool show_percentage;     // Show percentage text
    bool show_value;          // Show current value text
    bool show_label;          // Show custom label
    char label[256];          // Custom label text
    int font_size;            // Font size for text
    
    // Segmented bar properties
    int segment_count;        // Number of segments for segmented style
    float segment_gap;        // Gap between segments
    
    // Gradient properties
    Color gradient_start;     // Start color for gradient
    Color gradient_end;       // End color for gradient
    
    // State
    bool is_indeterminate;    // Indeterminate progress (animated without specific value)
    bool is_animated;         // Whether animation is enabled
    float animation_time;     // Current animation time
    
    // Callbacks
    void (*on_progress_complete)(struct LoadingBar* bar);
    void (*on_progress_change)(struct LoadingBar* bar, float old_progress, float new_progress);
} LoadingBar;

// Core Functions
LoadingBar LoadingBar_Create(Rectangle rect, LoadingBarStyle style);
bool LoadingBar_Init(LoadingBar* bar, Rectangle rect, LoadingBarStyle style);
void LoadingBar_Update(LoadingBar* bar);
void LoadingBar_Draw(const LoadingBar* bar);
void LoadingBar_Destroy(LoadingBar* bar);

// Progress Management
void LoadingBar_SetProgress(LoadingBar* bar, float progress);
void LoadingBar_SetValue(LoadingBar* bar, float value);
void LoadingBar_SetRange(LoadingBar* bar, float min_value, float max_value);
float LoadingBar_GetProgress(const LoadingBar* bar);
float LoadingBar_GetValue(const LoadingBar* bar);
void LoadingBar_Reset(LoadingBar* bar);

// Style Configuration
void LoadingBar_SetStyle(LoadingBar* bar, LoadingBarStyle style);
void LoadingBar_SetDirection(LoadingBar* bar, LoadingBarDirection direction);
void LoadingBar_SetAnimation(LoadingBar* bar, LoadingBarAnimation animation);
void LoadingBar_SetAnimationSpeed(LoadingBar* bar, float speed);

// Visual Customization
void LoadingBar_SetColors(LoadingBar* bar, Color background, Color progress, Color border);
void LoadingBar_SetProgressColor(LoadingBar* bar, Color color);
void LoadingBar_SetBackgroundColor(LoadingBar* bar, Color color);
void LoadingBar_SetBorderColor(LoadingBar* bar, Color color);
void LoadingBar_SetTextColor(LoadingBar* bar, Color color);

// Dimensions
void LoadingBar_SetThickness(LoadingBar* bar, float thickness);
void LoadingBar_SetBorderWidth(LoadingBar* bar, float width);
void LoadingBar_SetCornerRadius(LoadingBar* bar, float radius);

// Text Display
void LoadingBar_SetShowPercentage(LoadingBar* bar, bool show);
void LoadingBar_SetShowValue(LoadingBar* bar, bool show);
void LoadingBar_SetShowLabel(LoadingBar* bar, bool show);
void LoadingBar_SetLabel(LoadingBar* bar, const char* label);
void LoadingBar_SetFontSize(LoadingBar* bar, int size);

// Segmented Bar
void LoadingBar_SetSegmentCount(LoadingBar* bar, int count);
void LoadingBar_SetSegmentGap(LoadingBar* bar, float gap);

// Gradient
void LoadingBar_SetGradient(LoadingBar* bar, Color start, Color end);

// State Management
void LoadingBar_SetIndeterminate(LoadingBar* bar, bool indeterminate);
void LoadingBar_SetAnimated(LoadingBar* bar, bool animated);

// Callbacks
void LoadingBar_SetOnCompleteCallback(LoadingBar* bar, void (*callback)(LoadingBar* bar));
void LoadingBar_SetOnChangeCallback(LoadingBar* bar, void (*callback)(LoadingBar* bar, float old_progress, float new_progress));

// Utility Functions
bool LoadingBar_IsComplete(const LoadingBar* bar);
bool LoadingBar_IsIndeterminate(const LoadingBar* bar);
bool LoadingBar_IsAnimated(const LoadingBar* bar);

// Animation Helpers
void LoadingBar_StartAnimation(LoadingBar* bar);
void LoadingBar_StopAnimation(LoadingBar* bar);
void LoadingBar_PauseAnimation(LoadingBar* bar);
void LoadingBar_ResumeAnimation(LoadingBar* bar);

#endif // LOADING_BAR_H
