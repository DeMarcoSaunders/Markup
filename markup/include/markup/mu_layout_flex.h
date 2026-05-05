#ifndef MU_LAYOUT_FLEX_H
#define MU_LAYOUT_FLEX_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct MuFlexLayoutState {
    MuFlexDirection direction;
    MuFlexJustify justify;
    MuFlexAlign align_items;
    float gap;
    MuRect padding; /* x=left y=top w=right h=bottom as l,t,r,b OR use x,y,w,h as L,T,R,B - we use l,t,r,b in x,y,w,h */
    float pad_left, pad_top, pad_right, pad_bottom;
} MuFlexLayoutState;

/* Stored in panel/box node state alongside MuFlexLayoutState */
void mu_layout_flex_run(MuContext *ctx, MuNode *container);
void mu_layout_run(MuContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
