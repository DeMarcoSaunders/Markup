#ifndef THEME_H
#define THEME_H

#include "raylib.h"

typedef enum {
    BORDER_SOLID,
    BORDER_DASHED,
    BORDER_DOTTED,
    BORDER_DOUBLE
} BorderStyle;

typedef enum {
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT
} TextAlign;

typedef enum {
    TEXT_VERTICAL_TOP,
    TEXT_VERTICAL_CENTER,
    TEXT_VERTICAL_BOTTOM
} TextVerticalAlign;

typedef enum {
    COMPONENT_BUTTON,
    COMPONENT_PANEL,
    COMPONENT_SIDEBAR,
    COMPONENT_SLIDER,
    COMPONENT_MODAL,
    COMPONENT_TEXT_INPUT,
    COMPONENT_TEXT_AREA,
    COMPONENT_CHECKBOX,
    COMPONENT_RADIO_BUTTON,
    COMPONENT_COUNT
} ComponentType;

typedef enum {
    STATE_DEFAULT,
    STATE_HOVER,
    STATE_PRESSED,
    STATE_ACTIVE,
    STATE_DISABLED,
    STATE_COUNT
} ComponentState;

typedef struct {
    float top, right, bottom, left;
} EdgeValues;

typedef struct {
    float top_left, top_right, bottom_right, bottom_left;
} CornerRadius;

// Generic color palette for different states
typedef struct {
    Color background;
    Color border;
    Color text;
    Color accent;
    Color shadow;
} ColorPalette;

// Generic styling properties that apply to all components
typedef struct {
    ColorPalette colors[STATE_COUNT];
    EdgeValues border_width;
    EdgeValues outline_width;
    EdgeValues padding;
    EdgeValues margin;
    CornerRadius border_radius;
    BorderStyle border_style;
    Vector2 shadow_offset;
    float shadow_blur;
    float shadow_spread;
    float opacity;
    int z_index;
    int font_size;
    TextAlign text_align;
    TextVerticalAlign text_vertical_align;
    float line_height;
    float letter_spacing;
} ComponentStyle;

// Component-specific properties for special cases
typedef struct {
    float width;
    float item_height;
    float collapse_width;
    bool auto_collapse;
} SidebarProperties;

typedef struct {
    float track_height;
    float handle_size;
    float min_width;
    float step_size;
    bool show_value;
    float track_radius;
    float handle_radius;
} SliderProperties;

typedef struct {
    float overlay_opacity;
    float min_width;
    float min_height;
    float max_width_percent;
    float max_height_percent;
    bool close_on_overlay_click;
    bool show_close_button;
    float close_button_size;
    bool use_blur_effect;
    float blur_strength;
} ModalProperties;

typedef struct {
    float cursor_width;
    float cursor_blink_speed;
    float selection_opacity;
    bool show_placeholder;
    bool auto_select_on_focus;
    int max_length;
    float scroll_speed;
} TextInputProperties;

typedef struct {
    float line_height;
    float scroll_bar_width;
    bool show_line_numbers;
    bool word_wrap;
    bool show_scroll_bars;
    float scroll_speed;
    int tab_size;
    bool auto_indent;
} TextAreaProperties;

typedef struct {
    float box_size;
    float label_spacing;
    bool show_check_mark;
    float check_mark_thickness;
    float animation_speed;
} CheckboxProperties;

typedef struct {
    float circle_size;
    float dot_size;
    float label_spacing;
    float animation_speed;
} RadioButtonProperties;

// Font management structures
typedef struct {
    char name[64];          // Font display name (e.g., "Roboto Regular")
    char filename[128];     // Font file name (e.g., "Roboto-Regular.ttf")
    char filepath[256];     // Full path to font file
    Font font;              // Loaded raylib Font
    bool is_loaded;         // Whether the font is currently loaded
    bool is_default;        // Whether this is the default system font
} ThemeFont;

typedef struct {
    ThemeFont* fonts;       // Dynamic array of available fonts
    int font_count;         // Number of fonts available
    int current_font_index; // Index of currently active font
    char fonts_directory[256]; // Directory to scan for fonts
    bool auto_scan_fonts;   // Whether to automatically scan for new fonts
} FontManager;

typedef struct {
    // Generic component styles - scalable for any component
    ComponentStyle components[COMPONENT_COUNT];
    
    // Button variants (using the same ComponentStyle structure)
    ComponentStyle primary_button;
    ComponentStyle secondary_button;
    ComponentStyle destructive_button;
    
    // Component-specific properties for special cases
    SidebarProperties sidebar_props;
    SliderProperties slider_props;
    ModalProperties modal_props;
    TextInputProperties text_input_props;
    TextAreaProperties text_area_props;
    CheckboxProperties checkbox_props;
    RadioButtonProperties radio_button_props;
    
    // Font management
    FontManager font_manager;
    
    // Global theme settings
    float transition_duration;
} Theme;

// Global theme instance
extern Theme AppTheme;

// Function declarations
void Theme_Init(void);
void Theme_Init_Default(void);
ComponentStyle* Theme_GetComponentStyle(ComponentType type);
ColorPalette* Theme_GetComponentColors(ComponentType type, ComponentState state);

// Font management functions
void Theme_InitFontManager(const char* fonts_directory);
void Theme_ScanFonts(void);
bool Theme_LoadFont(int font_index, int font_size);
void Theme_UnloadFont(int font_index);
void Theme_SetCurrentFont(int font_index);
Font Theme_GetCurrentFont(void);
Font Theme_GetFont(int font_index);
const char* Theme_GetFontName(int font_index);
int Theme_GetFontCount(void);
int Theme_GetCurrentFontIndex(void);
int Theme_FindFontByName(const char* font_name);
bool Theme_AddCustomFont(const char* name, const char* filepath);
void Theme_CleanupFonts(void);

// Font utility functions
bool Theme_IsFontLoaded(int font_index);
void Theme_ReloadAllFonts(int font_size);
void Theme_SetFontsDirectory(const char* directory);
const char* Theme_GetFontsDirectory(void);

#endif // THEME_H