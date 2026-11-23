#include "../include/markup/mu_render.h"
#include "../include/markup/mu_text.h"
#include <stdio.h> /* snprintf -- MSVC accepts it undeclared, clang does not */
#include <string.h>
#include <stdlib.h>

static float text_line_height(MuRenderContext *rc, const MuTextStyle *style) {
    MuTextMetrics m;
    mu_text_measure(rc, "Ay", style, &m);
    if (m.height > 0.f) return m.height;
    return style && style->size > 0.f ? style->size * 1.25f : 20.f;
}

static bool is_space_byte(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static const char *skip_spaces(const char *p) {
    while (p && is_space_byte((unsigned char)*p)) p++;
    return p;
}

static const char *next_word(const char *p) {
    p = skip_spaces(p);
    if (!p || !*p) return p;
    while (*p && !is_space_byte((unsigned char)*p)) p++;
    return p;
}

static void copy_range(char *dst, size_t cap, const char *start, const char *end) {
    if (!dst || cap == 0) return;
    size_t n = end > start ? (size_t)(end - start) : 0;
    if (n >= cap) n = cap - 1;
    if (n > 0) memcpy(dst, start, n);
    dst[n] = '\0';
}

void mu_text_measure_wrapped(MuRenderContext *rc, const char *text, const MuTextStyle *style, float max_width,
                             MuTextMetrics *out) {
    if (!out) return;
    out->width = 0.f;
    out->height = 0.f;
    if (!text) return;

    if (max_width < 8.f) {
        mu_text_measure(rc, text, style, out);
        return;
    }

    const float line_h = text_line_height(rc, style);
    const char *p = text;
    float max_w = 0.f;
    int lines = 0;

    char line[512];
    char word[256];
    line[0] = '\0';

    while (*p) {
        const char *w0 = skip_spaces(p);
        if (!*w0) break;
        const char *w1 = next_word(w0);
        copy_range(word, sizeof(word), w0, w1);

        char trial[768];
        if (line[0]) {
            snprintf(trial, sizeof(trial), "%s %s", line, word);
        } else {
            snprintf(trial, sizeof(trial), "%s", word);
        }

        MuTextMetrics tm;
        mu_text_measure(rc, trial, style, &tm);

        if (line[0] && tm.width > max_width) {
            MuTextMetrics lm;
            mu_text_measure(rc, line, style, &lm);
            if (lm.width > max_w) max_w = lm.width;
            lines++;
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", trial);
        }

        p = w1;
    }

    if (line[0]) {
        MuTextMetrics lm;
        mu_text_measure(rc, line, style, &lm);
        if (lm.width > max_w) max_w = lm.width;
        lines++;
    }

    if (lines <= 0) {
        mu_text_measure(rc, text, style, out);
        return;
    }

    out->width = max_w;
    out->height = line_h * (float)lines;
}

void mu_draw_text_wrapped(MuRenderContext *rc, const char *text, MuRect bounds, const MuTextStyle *style,
                          MuColor fg) {
    if (!text || bounds.w < 1.f || bounds.h < 1.f) return;

    const float pad = 2.f;
    const float max_width = bounds.w - pad * 2.f;
    if (max_width < 4.f) return;

    const float line_h = text_line_height(rc, style);
    const char *p = text;
    float y = bounds.y + pad;

    char line[512];
    char word[256];
    line[0] = '\0';

    while (*p && y + line_h <= bounds.y + bounds.h + 0.5f) {
        const char *w0 = skip_spaces(p);
        if (!*w0) break;
        const char *w1 = next_word(w0);
        copy_range(word, sizeof(word), w0, w1);

        char trial[768];
        if (line[0]) {
            snprintf(trial, sizeof(trial), "%s %s", line, word);
        } else {
            snprintf(trial, sizeof(trial), "%s", word);
        }

        MuTextMetrics tm;
        mu_text_measure(rc, trial, style, &tm);

        if (line[0] && tm.width > max_width) {
            mu_draw_text(rc, line, bounds.x + pad, y, style, fg);
            y += line_h;
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", trial);
        }

        p = w1;
    }

    if (line[0] && y + line_h <= bounds.y + bounds.h + 0.5f)
        mu_draw_text(rc, line, bounds.x + pad, y, style, fg);
}

void mu_draw_text_ellipsis(MuRenderContext *rc, const char *text, MuRect bounds,
                           const MuTextStyle *style, MuColor fg) {
    if (!text || bounds.w <= 0.f || bounds.h <= 0.f) return;
    size_t len = strcspn(text, "\r\n");
    char *line = (char *)malloc(len + 4u);
    if (!line) return;
    memcpy(line, text, len);
    line[len] = '\0';
    MuTextMetrics tm;
    mu_text_measure(rc, line, style, &tm);
    if (tm.width > bounds.w || text[len]) {
        const char *ellipsis = "\xE2\x80\xA6";
        mu_text_measure(rc, ellipsis, style, &tm);
        if (tm.width > bounds.w) { free(line); return; }
        size_t lo = 0, hi = len;
        while (lo < hi) {
            size_t mid = lo + (hi - lo + 1u) / 2u;
            /* Round up to a UTF-8 boundary. lo and hi always are boundaries. */
            while (mid < hi && ((unsigned char)text[mid] & 0xC0u) == 0x80u) mid++;
            memcpy(line, text, mid);
            memcpy(line + mid, ellipsis, 4u);
            mu_text_measure(rc, line, style, &tm);
            if (tm.width <= bounds.w) lo = mid;
            else {
                hi = mid - 1u;
                while (hi > lo && ((unsigned char)text[hi] & 0xC0u) == 0x80u) hi--;
            }
        }
        memcpy(line, text, lo);
        memcpy(line + lo, ellipsis, 4u);
    }
    mu_push_scissor(rc, bounds);
    mu_draw_text(rc, line, bounds.x, bounds.y, style, fg);
    mu_pop_scissor(rc);
    free(line);
}
