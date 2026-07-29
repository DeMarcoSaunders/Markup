#ifndef MU_TEXT_H
#define MU_TEXT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuNode MuNode;

#define MU_FONT_DEFAULT 0u
#define MU_FONT_MAX 16u

#define MU_TEXT_WEIGHT_NORMAL 400
#define MU_TEXT_WEIGHT_BOLD 700

#define MU_TEXT_INHERIT_SIZE 0.f
#define MU_TEXT_INHERIT_WEIGHT 0
#define MU_TEXT_INHERIT_ITALIC (-1)
#define MU_TEXT_INHERIT_SPACING (-1.f)

typedef struct MuTextStyle {
    float size;
    int weight;
    int italic;
    float letter_spacing;
    uint32_t font_id;
} MuTextStyle;

void mu_text_style_init(MuTextStyle *t);
void mu_text_style_merge(MuTextStyle *base, const MuTextStyle *override);

void mu_node_set_text_size(MuNode *node, float size);
void mu_node_set_text_weight(MuNode *node, int weight);
void mu_node_set_text_bold(MuNode *node, bool bold);
void mu_node_set_text_italic(MuNode *node, bool italic);
void mu_node_set_text_letter_spacing(MuNode *node, float spacing);
void mu_node_set_text_font(MuNode *node, uint32_t font_id);

#ifdef __cplusplus
}
#endif

#endif
