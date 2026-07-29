#ifndef MU_LAYOUT_H
#define MU_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

/** Negative flex_basis means "auto" (intrinsic measure). */
#define MU_FLEX_BASIS_AUTO (-1.f)

typedef enum MuFlexDirection {
    MU_FLEX_ROW,
    MU_FLEX_COLUMN,
} MuFlexDirection;

typedef enum MuFlexAlign {
    MU_ALIGN_START,
    MU_ALIGN_CENTER,
    MU_ALIGN_END,
    MU_ALIGN_STRETCH,
} MuFlexAlign;

typedef enum MuFlexJustify {
    MU_JUSTIFY_START,
    MU_JUSTIFY_CENTER,
    MU_JUSTIFY_END,
    MU_JUSTIFY_SPACE_BETWEEN,
    MU_JUSTIFY_SPACE_AROUND,
} MuFlexJustify;

typedef enum MuAlignSelf {
    MU_ALIGN_SELF_AUTO = 0,
    MU_ALIGN_SELF_START,
    MU_ALIGN_SELF_CENTER,
    MU_ALIGN_SELF_END,
    MU_ALIGN_SELF_STRETCH,
} MuAlignSelf;

/**
 * CSS-like layout properties stored on every MuNode.
 * Container fields apply when MU_NODE_FLEX_CONTAINER is set.
 */
typedef struct MuLayoutStyle {
    float flex_grow;
    float flex_shrink;
    float flex_basis;

    float margin_top, margin_right, margin_bottom, margin_left;

    float min_width, min_height;
    float max_width, max_height;

    MuAlignSelf align_self;

    MuFlexDirection flex_direction;
    MuFlexJustify justify_content;
    MuFlexAlign align_items;
    float gap;
    int flex_wrap;
    float padding_top, padding_right, padding_bottom, padding_left;
} MuLayoutStyle;

#ifdef __cplusplus
}
#endif

#endif
