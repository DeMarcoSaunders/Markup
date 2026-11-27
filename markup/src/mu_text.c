#include "../include/markup/mu_core.h"
#include "../include/markup/mu_text.h"
#include "../include/markup/mu_layout_flex.h"

void mu_text_style_init(MuTextStyle *t) {
    if (!t) return;
    t->size = MU_TEXT_INHERIT_SIZE;
    t->weight = MU_TEXT_INHERIT_WEIGHT;
    t->italic = MU_TEXT_INHERIT_ITALIC;
    t->letter_spacing = MU_TEXT_INHERIT_SPACING;
    t->font_id = MU_FONT_DEFAULT;
}

void mu_text_style_merge(MuTextStyle *base, const MuTextStyle *override) {
    if (!base || !override) return;
    if (override->size > 0.f) base->size = override->size;
    if (override->weight > 0) base->weight = override->weight;
    if (override->italic >= 0) base->italic = override->italic;
    if (override->letter_spacing >= 0.f) base->letter_spacing = override->letter_spacing;
    if (override->font_id != MU_FONT_DEFAULT) base->font_id = override->font_id;
}

void mu_node_set_text_size(MuNode *node, float size) {
    if (!node) return;
    if (node->text.size == (size > 0.f ? size : MU_TEXT_INHERIT_SIZE)) return;
    node->text.size = size > 0.f ? size : MU_TEXT_INHERIT_SIZE;
    mu_layout_mark_dirty(node);
}

void mu_node_set_text_weight(MuNode *node, int weight) {
    if (!node) return;
    if (node->text.weight == (weight > 0 ? weight : MU_TEXT_INHERIT_WEIGHT)) return;
    node->text.weight = weight > 0 ? weight : MU_TEXT_INHERIT_WEIGHT;
    mu_layout_mark_dirty(node);
}

void mu_node_set_text_bold(MuNode *node, bool bold) {
    mu_node_set_text_weight(node, bold ? MU_TEXT_WEIGHT_BOLD : MU_TEXT_WEIGHT_NORMAL);
}

void mu_node_set_text_italic(MuNode *node, bool italic) {
    if (!node) return;
    if (node->text.italic == (italic ? 1 : 0)) return;
    node->text.italic = italic ? 1 : 0;
    mu_layout_mark_dirty(node);
}

void mu_node_set_text_letter_spacing(MuNode *node, float spacing) {
    if (!node) return;
    if (node->text.letter_spacing == (spacing >= 0.f ? spacing : MU_TEXT_INHERIT_SPACING)) return;
    node->text.letter_spacing = spacing >= 0.f ? spacing : MU_TEXT_INHERIT_SPACING;
    mu_layout_mark_dirty(node);
}

void mu_node_set_text_font(MuNode *node, uint32_t font_id) {
    if (!node) return;
    if (node->text.font_id == (font_id)) return;
    node->text.font_id = font_id;
    mu_layout_mark_dirty(node);
}
