#include "../include/markup/mu_skia.h"
#include "../include/markup/mu_image.h"
#include "../include/markup/mu_render.h"
#include "../include/markup/mu_text.h"

static MuRenderContext *g_mu_measure_rc;

void mu_render_bind_measure(MuRenderContext *rc) {
    g_mu_measure_rc = rc;
}

static MuRenderContext *measure_rc(MuRenderContext *rc) {
    return rc ? rc : g_mu_measure_rc;
}

#include "core/SkCanvas.h"
#include "core/SkColorFilter.h"
#include "core/SkSamplingOptions.h"
#include "core/SkBlendMode.h"
#include "core/SkColor.h"
#include "core/SkData.h"
#include "core/SkFont.h"
#include "core/SkFontMgr.h"
#include "core/SkFontStyle.h"
#include "core/SkImage.h"
#include "core/SkImageInfo.h"
#include "core/SkPaint.h"
#include "core/SkPixmap.h"
#include "core/SkRRect.h"
#include "core/SkRect.h"
#include "core/SkString.h"
#include "core/SkSurface.h"
#include "core/SkTypeface.h"
#if defined(_WIN32)
#include "ports/SkTypeface_win.h"
#endif

#include <cstring>
#include <memory>
#include <string>
#include <vector>

struct MuSkiaFontSlot {
    sk_sp<SkTypeface> typeface;
    std::string family;
};

struct MuSkiaImageSlot {
    sk_sp<SkImage> image;
    float width = 0.f;
    float height = 0.f;
};

struct MuSkiaBackend {
    sk_sp<SkSurface> surface;
    sk_sp<SkFontMgr> font_mgr;
    std::vector<MuSkiaFontSlot> font_slots;
    std::vector<MuSkiaImageSlot> image_slots;
    SkFont font;
    float font_size = 16.f;
    int width = 0;
    int height = 0;
    std::vector<SkRect> scissor_stack;
};

static sk_sp<SkFontMgr> mu_default_font_mgr() {
#if defined(_WIN32)
    return SkFontMgr_New_DirectWrite();
#else
    return SkFontMgr::RefEmpty();
#endif
}

static sk_sp<SkTypeface> mu_default_typeface(const sk_sp<SkFontMgr> &mgr) {
    sk_sp<SkTypeface> face = mgr->matchFamilyStyle(nullptr, SkFontStyle());
    if (!face) face = SkTypeface::MakeEmpty();
    return face;
}

static std::string typeface_family(const sk_sp<SkTypeface> &face) {
    SkString name;
    if (face) face->getFamilyName(&name);
    if (name.size() > 0) return std::string(name.c_str());
    return "sans-serif";
}

static void set_default_slot(MuSkiaBackend *b, sk_sp<SkTypeface> face) {
    if (!b) return;
    if (b->font_slots.empty())
        b->font_slots.push_back({});
    b->font_slots[0].typeface = face;
    b->font_slots[0].family = typeface_family(face);
    b->font.setTypeface(face);
}

static SkColor mu_to_sk(MuColor c) {
    return SkColorSetARGB(c.a, c.r, c.g, c.b);
}

static MuSkiaBackend *backend_of(MuRenderContext *rc) {
    return rc ? static_cast<MuSkiaBackend *>(rc->backend) : nullptr;
}

static SkCanvas *canvas_of(MuRenderContext *rc) {
    return rc ? static_cast<SkCanvas *>(rc->canvas) : nullptr;
}

static void ensure_surface(MuSkiaBackend *b, int w, int h) {
    if (!b || w <= 0 || h <= 0) return;
    if (b->surface && b->width == w && b->height == h) return;
    b->width = w;
    b->height = h;
    SkImageInfo info = SkImageInfo::MakeN32Premul(w, h);
    b->surface = SkSurfaces::Raster(info);
}

static void sync_font(MuSkiaBackend *b) {
    if (!b) return;
    if (!b->font_slots.empty() && b->font_slots[0].typeface)
        b->font.setTypeface(b->font_slots[0].typeface);
    b->font.setSize(b->font_size);
    b->font.setEdging(SkFont::Edging::kAntiAlias);
}

static const MuSkiaFontSlot &resolve_slot(const MuSkiaBackend *b, const MuTextStyle *style) {
    static const MuSkiaFontSlot empty{};
    if (!b || b->font_slots.empty()) return empty;
    if (!style || style->font_id == MU_FONT_DEFAULT || style->font_id >= b->font_slots.size())
        return b->font_slots[0];
    return b->font_slots[style->font_id];
}

static sk_sp<SkTypeface> resolve_typeface(MuSkiaBackend *b, const MuTextStyle *style) {
    if (!b) return nullptr;
    const MuSkiaFontSlot &slot = resolve_slot(b, style);

    int weight = MU_TEXT_WEIGHT_NORMAL;
    int italic = 0;
    if (style) {
        if (style->weight > 0) weight = style->weight;
        if (style->italic > 0) italic = 1;
    }

    SkFontStyle::Slant slant = italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant;
    SkFontStyle fs(weight, SkFontStyle::kNormal_Width, slant);

    if (b->font_mgr && !slot.family.empty()) {
        sk_sp<SkTypeface> face = b->font_mgr->matchFamilyStyle(slot.family.c_str(), fs);
        if (face) return face;
    }
    return slot.typeface ? slot.typeface : mu_default_typeface(b->font_mgr);
}

static SkFont make_sk_font(MuSkiaBackend *b, const MuTextStyle *style) {
    SkFont font;
    if (b) {
        font.setTypeface(resolve_typeface(b, style));
        float size = (style && style->size > 0.f) ? style->size : b->font_size;
        font.setSize(size);
    } else {
        sk_sp<SkFontMgr> mgr = mu_default_font_mgr();
        if (g_mu_measure_rc) {
            MuSkiaBackend *mb = backend_of(g_mu_measure_rc);
            if (mb) {
                font.setTypeface(resolve_typeface(mb, style));
                float size = (style && style->size > 0.f) ? style->size : mb->font_size;
                font.setSize(size);
                font.setEdging(SkFont::Edging::kAntiAlias);
                return font;
            }
        }
        font.setTypeface(mu_default_typeface(mgr));
        font.setSize((style && style->size > 0.f) ? style->size : 16.f);
    }
    font.setEdging(SkFont::Edging::kAntiAlias);
    return font;
}

extern "C" void mu_skia_render_init(MuRenderContext *rc, int width, int height) {
    if (!rc) return;
    std::memset(rc, 0, sizeof(*rc));
    auto *b = new MuSkiaBackend();
    b->font_mgr = mu_default_font_mgr();
    set_default_slot(b, mu_default_typeface(b->font_mgr));
    b->font_size = 16.f;
    sync_font(b);
    b->image_slots.push_back({});
    ensure_surface(b, width, height);
    rc->backend = b;
    rc->canvas = b->surface ? b->surface->getCanvas() : nullptr;
    rc->scissor_depth = 0;
}

extern "C" void mu_skia_render_shutdown(MuRenderContext *rc) {
    if (!rc) return;
    delete backend_of(rc);
    rc->backend = nullptr;
    rc->canvas = nullptr;
    rc->scissor_depth = 0;
}

extern "C" void mu_skia_resize(MuRenderContext *rc, int width, int height) {
    MuSkiaBackend *b = backend_of(rc);
    if (!b) return;
    ensure_surface(b, width, height);
    rc->canvas = b->surface ? b->surface->getCanvas() : nullptr;
}

extern "C" void mu_skia_begin_frame(MuRenderContext *rc, MuColor clear) {
    SkCanvas *canvas = canvas_of(rc);
    if (!canvas) return;
    canvas->save();
    canvas->clipRect(SkRect::MakeWH((float)backend_of(rc)->width, (float)backend_of(rc)->height));
    canvas->clear(mu_to_sk(clear));
    rc->scissor_depth = 0;
    backend_of(rc)->scissor_stack.clear();
}

extern "C" void mu_skia_end_frame(MuRenderContext *rc) {
    SkCanvas *canvas = canvas_of(rc);
    if (!canvas) return;
    while (rc->scissor_depth > 0) mu_pop_scissor(rc);
    canvas->restore();
}

extern "C" bool mu_skia_set_font_file(MuRenderContext *rc, const char *path, float size) {
    MuSkiaBackend *b = backend_of(rc);
    if (!b) return false;
    sk_sp<SkTypeface> face;
    if (path && path[0]) face = b->font_mgr->makeFromFile(path);
    if (!face) face = mu_default_typeface(b->font_mgr);
    set_default_slot(b, face);
    b->font_size = size > 0.f ? size : 16.f;
    sync_font(b);
    return face != nullptr;
}

extern "C" uint32_t mu_font_load_file(MuRenderContext *rc, const char *path, const char *family_name) {
    MuSkiaBackend *b = backend_of(rc);
    if (!b || !path || !path[0] || b->font_slots.size() >= MU_FONT_MAX) return MU_FONT_DEFAULT;
    sk_sp<SkTypeface> face = b->font_mgr->makeFromFile(path);
    if (!face) return MU_FONT_DEFAULT;

    MuSkiaFontSlot slot;
    slot.typeface = face;
    if (family_name && family_name[0])
        slot.family = family_name;
    else
        slot.family = typeface_family(face);
    b->font_slots.push_back(std::move(slot));
    return (uint32_t)(b->font_slots.size() - 1);
}

static const MuSkiaImageSlot *skia_resolve_image(const MuSkiaBackend *b, uint32_t image_id) {
    if (!b || image_id == MU_IMAGE_INVALID || image_id >= b->image_slots.size())
        return nullptr;
    if (!b->image_slots[image_id].image)
        return nullptr;
    return &b->image_slots[image_id];
}

static sk_sp<SkImage> skia_image_from_rgba(const MuImageRgba *rgba) {
    if (!rgba || !rgba->pixels || rgba->width <= 0 || rgba->height <= 0) return nullptr;
    const int stride = rgba->stride > 0 ? rgba->stride : rgba->width * 4;
    SkImageInfo info =
        SkImageInfo::Make(rgba->width, rgba->height, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
    return SkImages::RasterFromPixmapCopy(SkPixmap(info, rgba->pixels, (size_t)stride));
}

static sk_sp<SkImage> skia_decode_image_data(sk_sp<SkData> data, const char *path) {
    if (!data || data->size() == 0) return nullptr;

    sk_sp<SkImage> image = SkImages::DeferredFromEncodedData(data);
    if (image) {
        sk_sp<SkImage> raster = image->makeRasterImage();
        return raster ? raster : image;
    }

    MuImageRgba rgba = {0};
    if (mu_image_decode_rgba_memory(data->data(), data->size(), &rgba)) {
        image = skia_image_from_rgba(&rgba);
        mu_image_rgba_free(&rgba);
        return image;
    }

    if (path && mu_image_decode_rgba_file(path, &rgba)) {
        image = skia_image_from_rgba(&rgba);
        mu_image_rgba_free(&rgba);
        return image;
    }

    return nullptr;
}

extern "C" uint32_t mu_image_load_file(MuRenderContext *rc, const char *path) {
    MuSkiaBackend *b = backend_of(rc);
    if (!b || !path || !path[0] || b->image_slots.size() >= MU_IMAGE_MAX)
        return MU_IMAGE_INVALID;

    sk_sp<SkData> data = SkData::MakeFromFileName(path);
    if (!data) return MU_IMAGE_INVALID;
    sk_sp<SkImage> image = skia_decode_image_data(data, path);
    if (!image) return MU_IMAGE_INVALID;

    MuSkiaImageSlot slot;
    slot.image = image;
    slot.width = (float)image->width();
    slot.height = (float)image->height();
    b->image_slots.push_back(std::move(slot));
    return (uint32_t)(b->image_slots.size() - 1);
}

extern "C" bool mu_image_get_size(MuRenderContext *rc, uint32_t image_id, float *out_w, float *out_h) {
    rc = measure_rc(rc);
    MuSkiaBackend *b = backend_of(rc);
    const MuSkiaImageSlot *slot = skia_resolve_image(b, image_id);
    if (!slot) return false;
    if (out_w) *out_w = slot->width;
    if (out_h) *out_h = slot->height;
    return true;
}

extern "C" void mu_draw_image(MuRenderContext *rc, uint32_t image_id, MuRect dst, const MuDrawImageOpts *opts) {
    SkCanvas *canvas = canvas_of(rc);
    MuSkiaBackend *b = backend_of(rc);
    const MuSkiaImageSlot *slot = skia_resolve_image(b, image_id);
    if (!canvas || !slot || dst.w < 1.f || dst.h < 1.f) return;

    MuDrawImageOpts defaults;
    if (!opts) {
        mu_draw_image_opts_init(&defaults);
        opts = &defaults;
    }

    float tex_w = slot->width;
    float tex_h = slot->height;
    MuRect src_px;
    if (!mu_image_resolve_src(&opts->src, tex_w, tex_h, &src_px)) return;

    MuRect draw = mu_image_fit_dst(src_px.w, src_px.h, dst, opts->fit);
    SkRect srcRect = SkRect::MakeXYWH(src_px.x, src_px.y, src_px.w, src_px.h);
    SkRect dest = SkRect::MakeXYWH(draw.x, draw.y, draw.w, draw.h);

    canvas->save();
    if (opts->radius > 0.5f) {
        SkRRect rr;
        rr.setRectXY(dest, opts->radius, opts->radius);
        canvas->clipRRect(rr, true);
    }

    SkPaint paint;
    paint.setAntiAlias(true);
    if (opts->tint.a < 255 || opts->tint.r < 255 || opts->tint.g < 255 || opts->tint.b < 255) {
        paint.setColorFilter(SkColorFilters::Blend(mu_to_sk(opts->tint), SkBlendMode::kModulate));
    }
    SkSamplingOptions sampling(SkFilterMode::kLinear, SkMipmapMode::kNone);
    canvas->drawImageRect(slot->image, srcRect, dest, sampling, &paint, SkCanvas::kStrict_SrcRectConstraint);
    canvas->restore();
}

extern "C" const void *mu_skia_pixel_data(MuRenderContext *rc, int *out_w, int *out_h, int *out_row_bytes) {
    MuSkiaBackend *b = backend_of(rc);
    if (!b || !b->surface) return nullptr;
    SkPixmap pm;
    if (!b->surface->peekPixels(&pm)) return nullptr;
    if (out_w) *out_w = b->width;
    if (out_h) *out_h = b->height;
    if (out_row_bytes) *out_row_bytes = (int)pm.rowBytes();
    return pm.addr();
}

extern "C" const void *mu_present_pixel_data(MuRenderContext *rc, int *out_w, int *out_h, int *out_row_bytes) {
    return mu_skia_pixel_data(rc, out_w, out_h, out_row_bytes);
}

extern "C" void mu_draw_backdrop_blur(MuRenderContext *rc, MuRect area, float blur_radius, MuColor tint,
                                      float corner_radius) {
    /* Degraded: a flat tint, no blur.
     *
     * Skia can do this properly — saveLayer with SkImageFilters::Blur as the backdrop
     * filter is the intended API — but this tree cannot currently build or run the Skia
     * backend (no Skia available), so that code would ship unverified. Left as a tint
     * until it can actually be compiled and looked at. */
    (void)blur_radius;
    if (!rc || tint.a == 0) return;
    mu_draw_rect(rc, area, tint, MuColor{0, 0, 0, 0}, 0.f, corner_radius);
}

/* No cache while the blur above is a flat tint. When this becomes a real saveLayer blur,
 * an SkImage of the filtered layer is the natural thing to hold here. */
extern "C" bool mu_backdrop_cache_try(MuRenderContext *rc, const MuNode *node, MuRect area,
                                      float blur_radius, MuColor tint, float corner_radius) {
    (void)rc;
    (void)node;
    (void)area;
    (void)blur_radius;
    (void)tint;
    (void)corner_radius;
    return false;
}

extern "C" void mu_backdrop_cache_store(MuRenderContext *rc, const MuNode *node, MuRect area,
                                        float blur_radius, MuColor tint, float corner_radius) {
    (void)rc;
    (void)node;
    (void)area;
    (void)blur_radius;
    (void)tint;
    (void)corner_radius;
}

extern "C" void mu_backdrop_cache_drop(MuRenderContext *rc, const MuNode *node) {
    (void)rc;
    (void)node;
}

extern "C" void mu_text_measure(MuRenderContext *rc, const char *text, const MuTextStyle *style, MuTextMetrics *out) {
    rc = measure_rc(rc);
    if (!out) return;
    out->width = 0.f;
    out->height = 0.f;
    if (!text) return;

    MuTextStyle defaults;
    if (!style) {
        mu_text_style_init(&defaults);
        defaults.size = 16.f;
        style = &defaults;
    }

    MuSkiaBackend *b = backend_of(rc);
    SkFont font = b ? make_sk_font(b, style) : make_sk_font(nullptr, style);

    const size_t len = std::strlen(text);
    SkRect bounds;
    font.measureText(text, len, SkTextEncoding::kUTF8, &bounds);
    out->width = bounds.width();
    out->height = bounds.height();
}

extern "C" void mu_push_scissor(MuRenderContext *rc, MuRect r) {
    SkCanvas *canvas = canvas_of(rc);
    MuSkiaBackend *b = backend_of(rc);
    if (!canvas || !b || rc->scissor_depth >= 32) return;
    canvas->save();
    SkRect clip = SkRect::MakeLTRB(r.x, r.y, r.x + r.w, r.y + r.h);
    canvas->clipRect(clip, SkClipOp::kIntersect, true);
    b->scissor_stack.push_back(clip);
    rc->scissor_depth++;
}

extern "C" void mu_pop_scissor(MuRenderContext *rc) {
    SkCanvas *canvas = canvas_of(rc);
    MuSkiaBackend *b = backend_of(rc);
    if (!canvas || !b || rc->scissor_depth <= 0) return;
    canvas->restore();
    if (!b->scissor_stack.empty()) b->scissor_stack.pop_back();
    rc->scissor_depth--;
}

extern "C" void mu_draw_rect(MuRenderContext *rc, MuRect r, MuColor fill, MuColor border, float border_w,
                             float radius) {
    SkCanvas *canvas = canvas_of(rc);
    if (!canvas || r.w < 1.f || r.h < 1.f) return;

    SkRect rect = SkRect::MakeXYWH(r.x, r.y, r.w, r.h);
    if (fill.a > 0) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(mu_to_sk(fill));
        if (radius > 0.5f) {
            SkRRect rr;
            rr.setRectXY(rect, radius, radius);
            canvas->drawRRect(rr, paint);
        } else {
            canvas->drawRect(rect, paint);
        }
    }

    if (border_w > 0.f && border.a > 0) {
        SkPaint stroke;
        stroke.setAntiAlias(true);
        stroke.setStyle(SkPaint::kStroke_Style);
        stroke.setStrokeWidth(border_w);
        stroke.setColor(mu_to_sk(border));
        if (radius > 0.5f) {
            SkRRect rr;
            rr.setRectXY(rect, radius, radius);
            canvas->drawRRect(rr, stroke);
        } else {
            canvas->drawRect(rect, stroke);
        }
    }
}

extern "C" void mu_draw_text(MuRenderContext *rc, const char *text, float x, float y, const MuTextStyle *style,
                             MuColor fg) {
    SkCanvas *canvas = canvas_of(rc);
    MuSkiaBackend *b = backend_of(rc);
    if (!canvas || !text || !b) return;

    SkFont font = make_sk_font(b, style);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(mu_to_sk(fg));
    canvas->drawString(text, x, y + font.getSize() * 0.85f, font, paint);
}
