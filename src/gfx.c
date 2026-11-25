/* Drawing into the frame: smooth discs, rings and rounded boxes, images and text. */
#include <stdlib.h>
#include <string.h>

#include "c4.h"

uint32_t fb[VIEW_W * VIEW_H];

uint32_t mix(uint32_t a, uint32_t b, int t) {
    uint32_t rb = ((a & 0xff00ff) * (unsigned)(256 - t) + (b & 0xff00ff) * (unsigned)t) >> 8 & 0xff00ff;
    uint32_t g = ((a & 0xff00) * (unsigned)(256 - t) + (b & 0xff00) * (unsigned)t) >> 8 & 0xff00;
    return rb | g;
}

void blend(int x, int y, uint32_t c, int alpha) {
    if ((unsigned)x >= VIEW_W || (unsigned)y >= VIEW_H || alpha <= 0) return;
    uint32_t *p = &fb[y * VIEW_W + x];
    *p = alpha >= 256 ? c & 0xffffff : mix(*p, c, alpha);
}

void fill(uint32_t c) {
    for (int i = 0; i < VIEW_W * VIEW_H; ++i) fb[i] = c;
}

void rect(int x, int y, int w, int h, uint32_t c, int alpha) {
    int x0 = clampi(x, 0, VIEW_W), x1 = clampi(x + w, 0, VIEW_W), y0 = clampi(y, 0, VIEW_H), y1 = clampi(y + h, 0, VIEW_H);
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) blend(xx, yy, c, alpha);
}

void darken(int alpha) { rect(0, 0, VIEW_W, VIEW_H, 0, alpha); }

/* coverage 0-256 of a pixel whose centre is d px inside an edge (d < 0: outside) */
static int cover(float d) {
    if (d >= 0.5f) return 256;
    if (d <= -0.5f) return 0;
    return (int)((d + 0.5f) * 256);
}

void rrect(int x, int y, int w, int h, int r, uint32_t c, int alpha) {
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int yy = clampi(y, 0, VIEW_H); yy < clampi(y + h, 0, VIEW_H); ++yy)
        for (int xx = clampi(x, 0, VIEW_W); xx < clampi(x + w, 0, VIEW_W); ++xx) {
            float px = xx + 0.5f, py = yy + 0.5f;
            float cx = px < x + r ? x + r : px > x + w - r ? x + w - r : px;
            float cy = py < y + r ? y + r : py > y + h - r ? y + h - r : py;
            float dx = px - cx, dy = py - cy;
            int a = 256;
            if (dx || dy) a = cover(r - __builtin_sqrtf(dx * dx + dy * dy));
            blend(xx, yy, c, a * alpha >> 8);
        }
}

void ring(float cx, float cy, float r, float width, uint32_t c, int alpha) {
    int x0 = (int)(cx - r - 1), x1 = (int)(cx + r + 2), y0 = (int)(cy - r - 1), y1 = (int)(cy + r + 2);
    for (int y = clampi(y0, 0, VIEW_H); y < clampi(y1, 0, VIEW_H); ++y)
        for (int x = clampi(x0, 0, VIEW_W); x < clampi(x1, 0, VIEW_W); ++x) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = __builtin_sqrtf(dx * dx + dy * dy);
            int a = cover(r - d);
            if (width > 0) {
                int inner = cover(d - (r - width));
                a = a < inner ? a : inner;
            }
            if (a) blend(x, y, c, a * alpha >> 8);
        }
}

void disc(float cx, float cy, float r, uint32_t c, int alpha) { ring(cx, cy, r, 0, c, alpha); }

Img img_new(int w, int h) {
    Img i = { w, h, calloc((size_t)(w * h), 4) };
    return i;
}

void img(const Img *i, int x, int y, int alpha) {
    if (!i->px) return;
    for (int yy = 0; yy < i->h; ++yy) {
        int fy = y + yy;
        if ((unsigned)fy >= VIEW_H) continue;
        const uint32_t *s = i->px + yy * i->w;
        for (int xx = 0; xx < i->w; ++xx) {
            uint32_t p = s[xx];
            int a = (int)(p >> 24);
            if (a) blend(x + xx, fy, p, (a + (a >> 7)) * alpha >> 8);
        }
    }
}

static int glyph(int ch) { return ch < 32 || ch > 126 ? '?' - 32 : ch - 32; }

int text_w(const Font *f, const char *s) {
    int w = 0;
    for (; *s; ++s) w += f->adv[glyph((unsigned char)*s)];
    return w;
}

void text(const Font *f, const char *s, int x, int y, uint32_t c, int alpha) {
    for (; *s; ++s) {
        int g = glyph((unsigned char)*s);
        const uint8_t *src = f->px + f->x[g];
        for (int yy = 0; yy < f->h; ++yy)
            for (int xx = 0; xx < f->w[g]; ++xx) {
                int v = src[yy * f->atlas_w + xx];
                if (v) blend(x + f->left[g] + xx, y + yy, c, (v + (v >> 7)) * alpha >> 8);
            }
        x += f->adv[g];
    }
}

void text_c(const Font *f, const char *s, int cx, int y, uint32_t c, int alpha) { text(f, s, cx - text_w(f, s) / 2, y, c, alpha); }
