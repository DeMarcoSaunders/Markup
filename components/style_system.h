#ifndef STYLE_SYSTEM_H
#define STYLE_SYSTEM_H

#include "raylib.h"
#include "theme.h"
#include <stdbool.h>

// CSS-like specificity levels
typedef enum {
    SPECIFICITY_INHERITED = 0,
    SPECIFICITY_ELEMENT = 1,
    SPECIFICITY_CLASS = 10,
    SPECIFICITY_ID = 100,
    SPECIFICITY_INLINE = 1000,
    SPECIFICITY_IMPORTANT = 10000
} StyleSpecificity;

// CSS-like property types
typedef enum {
    PROPERTY_BACKGROUND_COLOR,
    PROPERTY_BORDER_COLOR,
    PROPERTY_TEXT_COLOR,
    PROPERTY_PADDING_TOP,
    PROPERTY_PADDING_RIGHT,
    PROPERTY_PADDING_BOTTOM,
    PROPERTY_PADDING_LEFT,
    PROPERTY_MARGIN_TOP,
    PROPERTY_MARGIN_RIGHT,
    PROPERTY_MARGIN_BOTTOM,
    PROPERTY_MARGIN_LEFT,
    PROPERTY_BORDER_WIDTH_TOP,
    PROPERTY_BORDER_WIDTH_RIGHT,
    PROPERTY_BORDER_WIDTH_BOTTOM,
    PROPERTY_BORDER_WIDTH_LEFT,
    PROPERTY_BORDER_RADIUS_TOP_LEFT,
    PROPERTY_BORDER_RADIUS_TOP_RIGHT,
    PROPERTY_BORDER_RADIUS_BOTTOM_RIGHT,
    PROPERTY_BORDER_RADIUS_BOTTOM_LEFT,
    PROPERTY_FONT_SIZE,
    PROPERTY_OPACITY,
    PROPERTY_Z_INDEX,
    PROPERTY_WIDTH,
    PROPERTY_HEIGHT,
    PROPERTY_DISPLAY,
    PROPERTY_POSITION,
    PROPERTY_TOP,
    PROPERTY_RIGHT,
    PROPERTY_BOTTOM,
    PROPERTY_LEFT,
    PROPERTY_FLEX_GROW,
    PROPERTY_FLEX_SHRINK,
    PROPERTY_FLEX_BASIS,
    PROPERTY_FLEX_DIRECTION,
    PROPERTY_JUSTIFY_CONTENT,
    PROPERTY_ALIGN_ITEMS,
    PROPERTY_GAP,
    PROPERTY_COUNT
} StyleProperty;

// CSS-like value types
typedef enum {
    VALUE_TYPE_NONE,
    VALUE_TYPE_COLOR,
    VALUE_TYPE_FLOAT,
    VALUE_TYPE_INT,
    VALUE_TYPE_STRING,
    VALUE_TYPE_PERCENTAGE,
    VALUE_TYPE_PIXELS,
    VALUE_TYPE_EM,
    VALUE_TYPE_REM,
    VALUE_TYPE_AUTO,
    VALUE_TYPE_INHERIT,
    VALUE_TYPE_INITIAL
} ValueType;

// CSS-like value union
typedef union {
    Color color;
    float float_value;
    int int_value;
    char* string_value;
} StyleValue;

// CSS-like property definition
typedef struct {
    StyleProperty property;
    StyleValue value;
    ValueType value_type;
    StyleSpecificity specificity;
    bool is_important;
    bool is_inherited;
} StylePropertyDef;

// CSS-like rule set
typedef struct {
    char* selector;
    StylePropertyDef* properties;
    int property_count;
    int property_capacity;
    StyleSpecificity specificity;
} StyleRule;

// CSS-like stylesheet
typedef struct {
    StyleRule* rules;
    int rule_count;
    int rule_capacity;
    char* name;
} Stylesheet;

// Computed style cache
typedef struct {
    StyleValue computed_values[PROPERTY_COUNT];
    bool is_valid;
    unsigned int cache_version;
} ComputedStyle;

// Style system context
typedef struct {
    Stylesheet* global_stylesheet;
    Stylesheet* component_stylesheets[COMPONENT_COUNT];
    ComputedStyle* computed_cache;
    int cache_size;
    unsigned int global_cache_version;
} StyleSystem;

// Style system functions
bool StyleSystem_Init(StyleSystem* system);
void StyleSystem_Destroy(StyleSystem* system);

// Stylesheet management
Stylesheet* StyleSystem_CreateStylesheet(const char* name);
bool StyleSystem_AddStylesheet(StyleSystem* system, Stylesheet* stylesheet);
bool StyleSystem_RemoveStylesheet(StyleSystem* system, const char* name);
Stylesheet* StyleSystem_GetStylesheet(StyleSystem* system, const char* name);

// Rule management
bool Stylesheet_AddRule(Stylesheet* stylesheet, const char* selector);
bool Stylesheet_AddProperty(Stylesheet* stylesheet, const char* selector, StyleProperty property, StyleValue value, ValueType value_type);
bool Stylesheet_RemoveRule(Stylesheet* stylesheet, const char* selector);
bool Stylesheet_ClearRules(Stylesheet* stylesheet);

// CSS-like selectors
bool StyleSystem_MatchesSelector(const char* selector, ComponentType component_type, const char* class_name, const char* id);
StyleSpecificity StyleSystem_CalculateSpecificity(const char* selector);

// Computed style calculation
ComputedStyle* StyleSystem_GetComputedStyle(StyleSystem* system, ComponentType component_type, const char* class_name, const char* id);
bool StyleSystem_InvalidateCache(StyleSystem* system);
bool StyleSystem_UpdateComputedStyles(StyleSystem* system);

// CSS-like shorthand properties
bool StyleSystem_SetPadding(StyleSystem* system, const char* selector, float top, float right, float bottom, float left);
bool StyleSystem_SetMargin(StyleSystem* system, const char* selector, float top, float right, float bottom, float left);
bool StyleSystem_SetBorder(StyleSystem* system, const char* selector, float width, Color color);
bool StyleSystem_SetBorderRadius(StyleSystem* system, const char* selector, float top_left, float top_right, float bottom_right, float bottom_left);

// CSS-like units and values
float StyleSystem_ParseValue(const char* value_string, float base_value, float parent_value);
Color StyleSystem_ParseColor(const char* color_string);
bool StyleSystem_IsValidValue(const char* value_string, ValueType expected_type);

// CSS-like inheritance
bool StyleSystem_ShouldInherit(StyleProperty property);
StyleValue StyleSystem_GetInheritedValue(StyleProperty property, const ComputedStyle* parent_style);

// CSS-like cascade and specificity
StylePropertyDef* StyleSystem_ResolveProperty(StyleSystem* system, StyleProperty property, ComponentType component_type, const char* class_name, const char* id);
bool StyleSystem_CompareSpecificity(StyleSpecificity a, StyleSpecificity b);

// Utility functions
const char* StyleSystem_PropertyToString(StyleProperty property);
StyleProperty StyleSystem_StringToProperty(const char* property_string);
const char* StyleSystem_ValueTypeToString(ValueType value_type);
ValueType StyleSystem_StringToValueType(const char* value_type_string);

#endif // STYLE_SYSTEM_H
