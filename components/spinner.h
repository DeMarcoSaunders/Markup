#ifndef SPINNER_H
#define SPINNER_H

#include "raylib.h"
#include "component_base.h"
#include <stdbool.h>

// Spinner styles
typedef enum {
    SPINNER_STYLE_CIRCULAR,
    SPINNER_STYLE_DOTS,
    SPINNER_STYLE_PULSE,
    SPINNER_STYLE_WAVE,
    SPINNER_STYLE_BARS,
    SPINNER_STYLE_RING,
    SPINNER_STYLE_SPIRAL,
    SPINNER_STYLE_BOUNCE
} SpinnerStyle;

// Spinner sizes
typedef enum {
    SPINNER_SIZE_SMALL,
    SPINNER_SIZE_MEDIUM,
    SPINNER_SIZE_LARGE,
    SPINNER_SIZE_CUSTOM
} SpinnerSize;

typedef struct {
    // Base component
    ComponentBase base;
    
    // Visual properties
    SpinnerStyle style;
    SpinnerSize size;
    float custom_size;        // Custom size when SPINNER_SIZE_CUSTOM
    
    // Colors
    Color primary_color;      // Main spinner color
    Color secondary_color;    // Secondary color for some styles
    Color background_color;   // Background color (if any)
    
    // Animation properties
    float rotation_speed;     // Rotation speed in degrees per second
    float animation_speed;    // General animation speed multiplier
    float current_rotation;   // Current rotation angle
    float animation_time;     // Current animation time
    
    // Style-specific properties
    int dot_count;           // Number of dots for dot spinner
    int bar_count;           // Number of bars for bar spinner
    float bar_width;         // Width of bars
    float bar_gap;           // Gap between bars
    float ring_thickness;    // Thickness of ring spinner
    float pulse_scale;       // Current pulse scale
    float wave_offset;       // Wave animation offset
    
    // State
    bool is_spinning;        // Whether spinner is currently spinning
    bool is_visible;         // Whether spinner is visible
    
    // Text display
    bool show_text;          // Show loading text
    char text[256];          // Loading text
    int font_size;           // Font size for text
    Color text_color;        // Text color
    float text_offset;       // Offset of text from spinner
    
    // Callbacks
    void (*on_spin_start)(struct Spinner* spinner);
    void (*on_spin_stop)(struct Spinner* spinner);
} Spinner;

// Core Functions
Spinner Spinner_Create(Rectangle rect, SpinnerStyle style);
bool Spinner_Init(Spinner* spinner, Rectangle rect, SpinnerStyle style);
void Spinner_Update(Spinner* spinner);
void Spinner_Draw(const Spinner* spinner);
void Spinner_Destroy(Spinner* spinner);

// Style Configuration
void Spinner_SetStyle(Spinner* spinner, SpinnerStyle style);
void Spinner_SetSize(Spinner* spinner, SpinnerSize size);
void Spinner_SetCustomSize(Spinner* spinner, float size);

// Visual Customization
void Spinner_SetColors(Spinner* spinner, Color primary, Color secondary, Color background);
void Spinner_SetPrimaryColor(Spinner* spinner, Color color);
void Spinner_SetSecondaryColor(Spinner* spinner, Color color);
void Spinner_SetBackgroundColor(Spinner* spinner, Color color);

// Animation Control
void Spinner_SetRotationSpeed(Spinner* spinner, float speed);
void Spinner_SetAnimationSpeed(Spinner* spinner, float speed);
void Spinner_Start(Spinner* spinner);
void Spinner_Stop(Spinner* spinner);
void Spinner_Pause(Spinner* spinner);
void Spinner_Resume(Spinner* spinner);

// Style-specific Configuration
void Spinner_SetDotCount(Spinner* spinner, int count);
void Spinner_SetBarCount(Spinner* spinner, int count);
void Spinner_SetBarWidth(Spinner* spinner, float width);
void Spinner_SetBarGap(Spinner* spinner, float gap);
void Spinner_SetRingThickness(Spinner* spinner, float thickness);

// Text Display
void Spinner_SetShowText(Spinner* spinner, bool show);
void Spinner_SetText(Spinner* spinner, const char* text);
void Spinner_SetFontSize(Spinner* spinner, int size);
void Spinner_SetTextColor(Spinner* spinner, Color color);
void Spinner_SetTextOffset(Spinner* spinner, float offset);

// State Management
void Spinner_SetVisible(Spinner* spinner, bool visible);
void Spinner_SetSpinning(Spinner* spinner, bool spinning);

// Callbacks
void Spinner_SetOnStartCallback(Spinner* spinner, void (*callback)(Spinner* spinner));
void Spinner_SetOnStopCallback(Spinner* spinner, void (*callback)(Spinner* spinner));

// Utility Functions
bool Spinner_IsSpinning(const Spinner* spinner);
bool Spinner_IsVisible(const Spinner* spinner);
float Spinner_GetCurrentRotation(const Spinner* spinner);

// Preset Configurations
void Spinner_SetPresetSmall(Spinner* spinner);
void Spinner_SetPresetMedium(Spinner* spinner);
void Spinner_SetPresetLarge(Spinner* spinner);
void Spinner_SetPresetFast(Spinner* spinner);
void Spinner_SetPresetSlow(Spinner* spinner);

#endif // SPINNER_H
