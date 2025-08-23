#ifndef LAYOUT_ENGINE_H
#define LAYOUT_ENGINE_H

#include "raylib.h"
#include "component_base.h"
#include <stdbool.h>

// Layout types
typedef enum {
    LAYOUT_TYPE_NONE,
    LAYOUT_TYPE_FLEXBOX,
    LAYOUT_TYPE_GRID,
    LAYOUT_TYPE_ABSOLUTE,
    LAYOUT_TYPE_RELATIVE,
    LAYOUT_TYPE_FIXED
} LayoutType;

// Flexbox properties
typedef enum {
    FLEX_DIRECTION_ROW,
    FLEX_DIRECTION_ROW_REVERSE,
    FLEX_DIRECTION_COLUMN,
    FLEX_DIRECTION_COLUMN_REVERSE
} FlexDirection;

typedef enum {
    FLEX_WRAP_NOWRAP,
    FLEX_WRAP_WRAP,
    FLEX_WRAP_WRAP_REVERSE
} FlexWrap;

typedef enum {
    JUSTIFY_CONTENT_FLEX_START,
    JUSTIFY_CONTENT_FLEX_END,
    JUSTIFY_CONTENT_CENTER,
    JUSTIFY_CONTENT_SPACE_BETWEEN,
    JUSTIFY_CONTENT_SPACE_AROUND,
    JUSTIFY_CONTENT_SPACE_EVENLY
} JustifyContent;

typedef enum {
    ALIGN_ITEMS_STRETCH,
    ALIGN_ITEMS_FLEX_START,
    ALIGN_ITEMS_FLEX_END,
    ALIGN_ITEMS_CENTER,
    ALIGN_ITEMS_BASELINE
} AlignItems;

typedef enum {
    ALIGN_CONTENT_FLEX_START,
    ALIGN_CONTENT_FLEX_END,
    ALIGN_CONTENT_CENTER,
    ALIGN_CONTENT_SPACE_BETWEEN,
    ALIGN_CONTENT_SPACE_AROUND,
    ALIGN_CONTENT_STRETCH
} AlignContent;

// Grid properties
typedef enum {
    GRID_TEMPLATE_COLUMNS,
    GRID_TEMPLATE_ROWS,
    GRID_COLUMN_START,
    GRID_COLUMN_END,
    GRID_ROW_START,
    GRID_ROW_END,
    GRID_GAP_COLUMN,
    GRID_GAP_ROW
} GridProperty;

// Layout constraints
typedef struct {
    float min_width;
    float max_width;
    float min_height;
    float max_height;
    float preferred_width;
    float preferred_height;
    bool width_is_percentage;
    bool height_is_percentage;
} LayoutConstraints;

// Flexbox item properties
typedef struct {
    int order;
    float flex_grow;
    float flex_shrink;
    float flex_basis;
    AlignItems align_self;
    bool is_flexible;
} FlexItem;

// Grid item properties
typedef struct {
    int grid_column_start;
    int grid_column_end;
    int grid_row_start;
    int grid_row_end;
    int grid_column_span;
    int grid_row_span;
} GridItem;

// Layout context
typedef struct {
    LayoutType type;
    Rectangle container_rect;
    LayoutConstraints constraints;
    
    // Flexbox properties
    FlexDirection flex_direction;
    FlexWrap flex_wrap;
    JustifyContent justify_content;
    AlignItems align_items;
    AlignContent align_content;
    float gap;
    
    // Grid properties
    int grid_columns;
    int grid_rows;
    float* column_sizes;
    float* row_sizes;
    float grid_gap_column;
    float grid_gap_row;
    
    // Layout results
    ComponentBase** items;
    int item_count;
    Rectangle* item_rects;
    bool needs_recalculation;
} LayoutContext;

// Layout engine functions
bool LayoutEngine_Init(LayoutContext* context, LayoutType type, Rectangle container_rect);
void LayoutEngine_Destroy(LayoutContext* context);

// Layout type management
void LayoutEngine_SetLayoutType(LayoutContext* context, LayoutType type);
LayoutType LayoutEngine_GetLayoutType(const LayoutContext* context);

// Container management
void LayoutEngine_SetContainerRect(LayoutContext* context, Rectangle rect);
Rectangle LayoutEngine_GetContainerRect(const LayoutContext* context);
void LayoutEngine_SetConstraints(LayoutContext* context, LayoutConstraints constraints);
LayoutConstraints LayoutEngine_GetConstraints(const LayoutContext* context);

// Item management
bool LayoutEngine_AddItem(LayoutContext* context, ComponentBase* item);
bool LayoutEngine_RemoveItem(LayoutContext* context, ComponentBase* item);
bool LayoutEngine_ClearItems(LayoutContext* context);
int LayoutEngine_GetItemCount(const LayoutContext* context);
ComponentBase* LayoutEngine_GetItem(const LayoutContext* context, int index);
Rectangle LayoutEngine_GetItemRect(const LayoutContext* context, int index);

// Flexbox layout functions
void LayoutEngine_SetFlexDirection(LayoutContext* context, FlexDirection direction);
void LayoutEngine_SetFlexWrap(LayoutContext* context, FlexWrap wrap);
void LayoutEngine_SetJustifyContent(LayoutContext* context, JustifyContent justify);
void LayoutEngine_SetAlignItems(LayoutContext* context, AlignItems align);
void LayoutEngine_SetAlignContent(LayoutContext* context, AlignContent align);
void LayoutEngine_SetGap(LayoutContext* context, float gap);

// Grid layout functions
void LayoutEngine_SetGridColumns(LayoutContext* context, int columns);
void LayoutEngine_SetGridRows(LayoutContext* context, int rows);
void LayoutEngine_SetColumnSizes(LayoutContext* context, const float* sizes, int count);
void LayoutEngine_SetRowSizes(LayoutContext* context, const float* sizes, int count);
void LayoutEngine_SetGridGap(LayoutContext* context, float column_gap, float row_gap);

// Item-specific layout properties
void LayoutEngine_SetItemFlex(LayoutContext* context, int item_index, float grow, float shrink, float basis);
void LayoutEngine_SetItemAlignSelf(LayoutContext* context, int item_index, AlignItems align);
void LayoutEngine_SetItemOrder(LayoutContext* context, int item_index, int order);
void LayoutEngine_SetItemGridPosition(LayoutContext* context, int item_index, int column_start, int row_start, int column_span, int row_span);

// Layout calculation
bool LayoutEngine_CalculateLayout(LayoutContext* context);
bool LayoutEngine_NeedsRecalculation(const LayoutContext* context);
void LayoutEngine_InvalidateLayout(LayoutContext* context);

// Layout algorithms
bool LayoutEngine_CalculateFlexboxLayout(LayoutContext* context);
bool LayoutEngine_CalculateGridLayout(LayoutContext* context);
bool LayoutEngine_CalculateAbsoluteLayout(LayoutContext* context);

// Utility functions
float LayoutEngine_CalculateFlexBasis(const LayoutContext* context, int item_index);
float LayoutEngine_CalculateAvailableSpace(const LayoutContext* context, FlexDirection direction);
bool LayoutEngine_IsFlexItem(const LayoutContext* context, int item_index);
bool LayoutEngine_IsGridItem(const LayoutContext* context, int item_index);

// Layout debugging
void LayoutEngine_DebugPrintLayout(const LayoutContext* context);
void LayoutEngine_DebugDrawLayout(const LayoutContext* context);

#endif // LAYOUT_ENGINE_H
