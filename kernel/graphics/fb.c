/*
 * MettaOS — motor de framebuffer lineal. SA-14
 */
#include <graphics/fb.h>
#include "metta/vmm.h"
#include "log/klog.h"

struct multiboot2_tag_header {
    uint32_t type;
    uint32_t size;
};

struct multiboot2_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t reserved;
} __attribute__((packed));

static framebuffer_t g_fb;
static fb_dirty_t g_dirty;
static fb_phys_map_fn g_phys_map;

static void *fb_default_phys_map(uint64_t phys, uint64_t size)
{
    (void)size;
    return (void *)(uintptr_t)phys;
}

static fb_pixel_format_t fb_format_from_mb(uint8_t mb_type, uint8_t bpp)
{
    if (mb_type != FB_MB_TYPE_RGB)
        return FB_FORMAT_UNKNOWN;
    if (bpp == 32)
        return FB_FORMAT_XRGB8888;
    if (bpp == 24)
        return FB_FORMAT_RGB888;
    if (bpp == 16)
        return FB_FORMAT_RGB565;
    return FB_FORMAT_UNKNOWN;
}

static const struct multiboot2_tag_framebuffer *
fb_find_framebuffer_tag(const void *multiboot_info)
{
    const uint8_t *walk;
    uint32_t total_size;

    if (!multiboot_info)
        return NULL;

    total_size = *(const uint32_t *)multiboot_info;
    walk = (const uint8_t *)multiboot_info + 8;

    while ((uint32_t)(walk - (const uint8_t *)multiboot_info) < total_size) {
        const struct multiboot2_tag_header *hdr =
            (const struct multiboot2_tag_header *)walk;
        uint32_t tag_size;

        if (hdr->type == 0 && hdr->size == 8)
            break;
        if (hdr->size < 8)
            break;

        if (hdr->type == FB_MULTIBOOT_TAG_FRAMEBUFFER)
            return (const struct multiboot2_tag_framebuffer *)walk;

        tag_size = (hdr->size + 7u) & ~7u;
        walk += tag_size;
    }
    return NULL;
}

void fb_set_phys_mapper(fb_phys_map_fn mapper)
{
    g_phys_map = mapper;
}

int fb_init_from_multiboot(const void *multiboot_info)
{
    const struct multiboot2_tag_framebuffer *tag;
    fb_phys_map_fn map_fn;
    void *base;

    g_fb.ready = 0;
    g_dirty.count = 0;
    g_dirty.full_frame = 0;

    tag = fb_find_framebuffer_tag(multiboot_info);
    if (!tag)
        return -1;

    map_fn = g_phys_map ? g_phys_map : fb_default_phys_map;
    base = map_fn(tag->framebuffer_addr,
                  (uint64_t)tag->framebuffer_pitch * tag->framebuffer_height);
    if (!base)
        return -2;

    g_fb.phys_addr = tag->framebuffer_addr;
    g_fb.base = (uint32_t *)base;
    g_fb.width = tag->framebuffer_width;
    g_fb.height = tag->framebuffer_height;
    g_fb.pitch = tag->framebuffer_pitch;
    g_fb.bpp = tag->framebuffer_bpp;
    g_fb.mb_type = tag->framebuffer_type;
    g_fb.format = fb_format_from_mb(tag->framebuffer_type, tag->framebuffer_bpp);

    if (g_fb.format != FB_FORMAT_XRGB8888) {
        /* Rutas 16/24 bpp reservadas; compositor asume XRGB por ahora. */
        g_fb.ready = 0;
        return -3;
    }

    /* QEMU suele poner el FB en 0xfd000000 (MMIO); forzar identidad 2 MiB. */
    {
        uint64_t fb_bytes =
            (uint64_t)g_fb.pitch * (uint64_t)g_fb.height;
        metta_status_t mst =
            vmm_map_identity_range((metta_phys_addr_t)g_fb.phys_addr, fb_bytes);
        if (mst != METTA_OK) {
            KLOG_ERR("fb: map identity phys=%llx len=%llu st=%d\n",
                     (unsigned long long)g_fb.phys_addr,
                     (unsigned long long)fb_bytes, (int)mst);
            g_fb.ready = 0;
            return -4;
        }
    }

    g_fb.ready = 1;
    fb_mark_full_framebuffer_dirty();
    return 0;
}

const framebuffer_t *fb_get(void)
{
    return g_fb.ready ? &g_fb : NULL;
}

fb_dirty_t *fb_dirty_get(void)
{
    return &g_dirty;
}

int fb_clip_rect(fb_rect_t *r, const framebuffer_t *fb)
{
    int32_t x2, y2;

    if (!r || !fb || r->w <= 0 || r->h <= 0)
        return 0;

    if (r->x < 0) {
        r->w += r->x;
        r->x = 0;
    }
    if (r->y < 0) {
        r->h += r->y;
        r->y = 0;
    }

    x2 = r->x + r->w;
    y2 = r->y + r->h;
    if (x2 > (int32_t)fb->width)
        r->w = (int32_t)fb->width - r->x;
    if (y2 > (int32_t)fb->height)
        r->h = (int32_t)fb->height - r->y;

    return (r->w > 0 && r->h > 0);
}

void fb_rect_union(const fb_rect_t *a, const fb_rect_t *b, fb_rect_t *out)
{
    int32_t ax2, ay2, bx2, by2;

    if (!a || !b || !out)
        return;

    ax2 = a->x + a->w;
    ay2 = a->y + a->h;
    bx2 = b->x + b->w;
    by2 = b->y + b->h;

    out->x = (a->x < b->x) ? a->x : b->x;
    out->y = (a->y < b->y) ? a->y : b->y;
    ax2 = (ax2 > bx2) ? ax2 : bx2;
    ay2 = (ay2 > by2) ? ay2 : by2;
    out->w = ax2 - out->x;
    out->h = ay2 - out->y;
}

static void fb_put_pixel(framebuffer_t *fb, int32_t x, int32_t y, uint32_t color)
{
    uint8_t *row;
    uint32_t *px;

    row = (uint8_t *)fb->base + (uint64_t)y * fb->pitch;
    px = (uint32_t *)(row + (uint64_t)x * 4u);
    *px = color;
}

static void fb_dirty_push(const fb_rect_t *rect)
{
    fb_rect_t clipped;
    uint32_t i;

    if (!rect || g_dirty.full_frame)
        return;

    clipped = *rect;
    if (!fb_clip_rect(&clipped, &g_fb))
        return;

    for (i = 0; i < g_dirty.count; i++) {
        if (clipped.x == g_dirty.rects[i].x && clipped.y == g_dirty.rects[i].y &&
            clipped.w == g_dirty.rects[i].w && clipped.h == g_dirty.rects[i].h)
            return;
    }

    if (g_dirty.count >= FB_MAX_DIRTY_RECTS) {
        g_dirty.full_frame = 1;
        g_dirty.count = 0;
        return;
    }

    g_dirty.rects[g_dirty.count++] = clipped;
}

void fb_mark_dirty(const fb_rect_t *rect)
{
    if (!g_fb.ready)
        return;
    fb_dirty_push(rect);
}

void fb_mark_dirty_rect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    fb_rect_t r = { x, y, w, h };
    fb_mark_dirty(&r);
}

void fb_mark_full_framebuffer_dirty(void)
{
    g_dirty.full_frame = 1;
    g_dirty.count = 0;
}

void fb_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
    fb_rect_t clip;
    int32_t row, col;

    if (!g_fb.ready || w <= 0 || h <= 0)
        return;

    clip = (fb_rect_t){ x, y, w, h };
    if (!fb_clip_rect(&clip, &g_fb))
        return;

    for (row = clip.y; row < clip.y + clip.h; row++) {
        for (col = clip.x; col < clip.x + clip.w; col++)
            fb_put_pixel(&g_fb, col, row, color);
    }

    fb_dirty_push(&clip);
}

void fb_blit(int32_t dst_x, int32_t dst_y, int32_t src_w, int32_t src_h,
             const uint32_t *src_pixels, int32_t src_stride_px)
{
    fb_rect_t clip;
    int32_t row, col;
    const uint32_t *src_row;

    if (!g_fb.ready || !src_pixels || src_w <= 0 || src_h <= 0)
        return;

    if (src_stride_px <= 0)
        src_stride_px = src_w;

    clip = (fb_rect_t){ dst_x, dst_y, src_w, src_h };
    if (!fb_clip_rect(&clip, &g_fb))
        return;

    for (row = 0; row < clip.h; row++) {
        int32_t sy = clip.y + row;
        int32_t src_y = sy - dst_y;

        src_row = src_pixels + (uint64_t)src_y * (uint64_t)src_stride_px + (clip.x - dst_x);
        {
            void *dst = (uint8_t *)g_fb.base + (uint64_t)sy * g_fb.pitch + (uint64_t)clip.x * 4u;
            const void *src = src_row;
            uint64_t n = (uint64_t)clip.w;

            __asm__ volatile("rep movsl" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
        }
    }
    (void)col;

    fb_dirty_push(&clip);
}

static void fb_dirty_merge_pass(void)
{
    uint32_t i, j;
    int merged;

    if (g_dirty.full_frame || g_dirty.count < 2)
        return;

    do {
        merged = 0;
        for (i = 0; i < g_dirty.count; i++) {
            for (j = i + 1; j < g_dirty.count; j++) {
                fb_rect_t u;
                fb_rect_t a = g_dirty.rects[i];
                fb_rect_t b = g_dirty.rects[j];

                fb_rect_union(&a, &b, &u);
                if (u.w * u.h <= (a.w * a.h + b.w * b.h)) {
                    g_dirty.rects[i] = u;
                    g_dirty.rects[j] = g_dirty.rects[g_dirty.count - 1];
                    g_dirty.count--;
                    merged = 1;
                    break;
                }
            }
            if (merged)
                break;
        }
    } while (merged && g_dirty.count > 1);
}

void fb_present(void)
{
    if (!g_fb.ready)
        return;

    /*
     * SA-15 debe leer fb_dirty_get() antes de fb_present() si necesita rects.
     * Aquí fusionamos y reiniciamos el tracking para el siguiente frame.
     */
    if (!g_dirty.full_frame)
        fb_dirty_merge_pass();

    g_dirty.count = 0;
    g_dirty.full_frame = 0;
}