/*
 * MDE desktop — animaciones por tiempo monotónico (SA-22 / LG-06).
 * Sin floats (kernel freestanding: -mno-sse). Valores en milésimas (1000 = 1.0).
 */

#include <stddef.h>
#include <stdint.h>

#define MDE_ANIM_MAX_CHANNELS 64u

#define MDE_ANIM_CH_CTXMENU_FADE     60u
#define MDE_ANIM_CH_CTXMENU_SCALE    61u
#define MDE_ANIM_CH_LAUNCHER_FADE    62u
#define MDE_ANIM_CH_LAUNCHER_SCALE   63u

#define MDE_ANIM_OVERLAY_OPEN_MS_MIN 120u
#define MDE_ANIM_OVERLAY_OPEN_MS     140u
#define MDE_ANIM_OVERLAY_OPEN_MS_MAX 160u

/* Escala/opacidad en milésimas: 1000 = 1.0 */
#define MDE_ANIM_OVERLAY_SCALE_FROM    940
#define MDE_ANIM_OVERLAY_OPACITY_FROM    0
#define MDE_ANIM_OVERLAY_SCALE_TO     1000
#define MDE_ANIM_OVERLAY_OPACITY_TO   1000
#define MDE_ANIM_EASE_OUT                1u

typedef enum {
    MDE_EASE_LINEAR = 0,
    MDE_EASE_OUT_CUBIC,
} mde_ease_t;

typedef struct {
    uint8_t  active;
    int32_t  from;
    int32_t  to;
    int32_t  value;
    uint64_t start_ms;
    uint32_t duration_ms;
    mde_ease_t ease;
} mde_anim_channel_t;

static mde_anim_channel_t g_channels[MDE_ANIM_MAX_CHANNELS];

/* t_milli en [0,1000] → ease en [0,1000] */
static int32_t mde_ease_apply_milli(mde_ease_t ease, int32_t t_milli)
{
    if (t_milli <= 0) {
        return 0;
    }
    if (t_milli >= 1000) {
        return 1000;
    }

    switch (ease) {
    case MDE_EASE_OUT_CUBIC: {
        /* 1 - (1-t)^3  con t = t_milli/1000 */
        int32_t u = 1000 - t_milli;
        int64_t u3 = (int64_t)u * u * u;
        return (int32_t)(1000 - (u3 / (1000 * 1000)));
    }
    case MDE_EASE_LINEAR:
    default:
        return t_milli;
    }
}

void mde_anim_init(void)
{
    for (uint32_t i = 0; i < MDE_ANIM_MAX_CHANNELS; i++) {
        g_channels[i].active      = 0;
        g_channels[i].from        = 0;
        g_channels[i].to          = 0;
        g_channels[i].value       = 0;
        g_channels[i].start_ms    = 0;
        g_channels[i].duration_ms = 0;
        g_channels[i].ease        = MDE_EASE_LINEAR;
    }
}

int mde_anim_start(uint32_t channel_id, int32_t from, int32_t to,
                   uint32_t duration_ms, uint8_t ease_id, uint64_t now_ms)
{
    if (channel_id >= MDE_ANIM_MAX_CHANNELS || duration_ms == 0) {
        return -1;
    }

    mde_anim_channel_t *ch = &g_channels[channel_id];

    if (ch->active && ch->to == to && ch->from == ch->value) {
        return 0;
    }

    ch->active      = 1;
    ch->from        = from;
    ch->to          = to;
    ch->value       = from;
    ch->start_ms    = now_ms;
    ch->duration_ms = duration_ms;
    ch->ease        = (ease_id == 0) ? MDE_EASE_LINEAR : MDE_EASE_OUT_CUBIC;
    return 0;
}

void mde_anim_stop(uint32_t channel_id)
{
    if (channel_id >= MDE_ANIM_MAX_CHANNELS) {
        return;
    }
    g_channels[channel_id].active = 0;
}

void mde_anim_set_immediate(uint32_t channel_id, int32_t value)
{
    if (channel_id >= MDE_ANIM_MAX_CHANNELS) {
        return;
    }
    mde_anim_channel_t *ch = &g_channels[channel_id];
    ch->active      = 0;
    ch->from        = value;
    ch->to          = value;
    ch->value       = value;
    ch->start_ms    = 0;
    ch->duration_ms = 0;
}

int32_t mde_anim_value(uint32_t channel_id)
{
    if (channel_id >= MDE_ANIM_MAX_CHANNELS) {
        return 0;
    }
    return g_channels[channel_id].value;
}

int mde_anim_active(uint32_t channel_id)
{
    if (channel_id >= MDE_ANIM_MAX_CHANNELS) {
        return 0;
    }
    return (int)g_channels[channel_id].active;
}

uint32_t mde_anim_tick(uint64_t now_ms)
{
    uint32_t running = 0;

    for (uint32_t i = 0; i < MDE_ANIM_MAX_CHANNELS; i++) {
        mde_anim_channel_t *ch = &g_channels[i];

        if (!ch->active) {
            continue;
        }

        uint64_t elapsed;
        if (now_ms <= ch->start_ms) {
            elapsed = 0;
        } else {
            elapsed = now_ms - ch->start_ms;
        }

        if (elapsed >= ch->duration_ms) {
            ch->value  = ch->to;
            ch->active = 0;
        } else {
            int32_t t_milli = (int32_t)((elapsed * 1000ull) / (uint64_t)ch->duration_ms);
            int32_t e = mde_ease_apply_milli(ch->ease, t_milli);
            int64_t delta = (int64_t)ch->to - (int64_t)ch->from;
            ch->value = ch->from + (int32_t)((delta * e) / 1000);
            running++;
        }
    }

    return running;
}

void mde_anim_overlay_enter(uint32_t fade_ch, uint32_t scale_ch, uint64_t now_ms)
{
    uint32_t dur = MDE_ANIM_OVERLAY_OPEN_MS;
    if (dur < MDE_ANIM_OVERLAY_OPEN_MS_MIN) {
        dur = MDE_ANIM_OVERLAY_OPEN_MS_MIN;
    } else if (dur > MDE_ANIM_OVERLAY_OPEN_MS_MAX) {
        dur = MDE_ANIM_OVERLAY_OPEN_MS_MAX;
    }

    mde_anim_start(fade_ch,
                   mde_anim_value(fade_ch),
                   MDE_ANIM_OVERLAY_OPACITY_TO,
                   dur, MDE_ANIM_EASE_OUT, now_ms);
    mde_anim_start(scale_ch,
                   mde_anim_value(scale_ch),
                   MDE_ANIM_OVERLAY_SCALE_TO,
                   dur, MDE_ANIM_EASE_OUT, now_ms);
}

void mde_anim_overlay_dismiss(uint32_t fade_ch, uint32_t scale_ch, uint64_t now_ms)
{
    mde_anim_start(fade_ch,
                   mde_anim_value(fade_ch),
                   MDE_ANIM_OVERLAY_OPACITY_FROM,
                   MDE_ANIM_OVERLAY_OPEN_MS, MDE_ANIM_EASE_OUT, now_ms);
    mde_anim_start(scale_ch,
                   mde_anim_value(scale_ch),
                   MDE_ANIM_OVERLAY_SCALE_FROM,
                   MDE_ANIM_OVERLAY_OPEN_MS, MDE_ANIM_EASE_OUT, now_ms);
}

void mde_anim_overlay_reset_closed(uint32_t fade_ch, uint32_t scale_ch)
{
    mde_anim_set_immediate(fade_ch, MDE_ANIM_OVERLAY_OPACITY_FROM);
    mde_anim_set_immediate(scale_ch, MDE_ANIM_OVERLAY_SCALE_FROM);
}

int32_t mde_anim_ctxmenu_opacity(void)
{
    return mde_anim_value(MDE_ANIM_CH_CTXMENU_FADE);
}

int32_t mde_anim_ctxmenu_scale(void)
{
    return mde_anim_value(MDE_ANIM_CH_CTXMENU_SCALE);
}

int32_t mde_anim_launcher_opacity(void)
{
    return mde_anim_value(MDE_ANIM_CH_LAUNCHER_FADE);
}

int32_t mde_anim_launcher_scale(void)
{
    return mde_anim_value(MDE_ANIM_CH_LAUNCHER_SCALE);
}

void mde_anim_ctxmenu_enter(uint64_t now_ms)
{
    mde_anim_overlay_enter(MDE_ANIM_CH_CTXMENU_FADE, MDE_ANIM_CH_CTXMENU_SCALE, now_ms);
}

void mde_anim_ctxmenu_dismiss(uint64_t now_ms)
{
    mde_anim_overlay_dismiss(MDE_ANIM_CH_CTXMENU_FADE, MDE_ANIM_CH_CTXMENU_SCALE, now_ms);
}

void mde_anim_launcher_enter(uint64_t now_ms)
{
    mde_anim_overlay_enter(MDE_ANIM_CH_LAUNCHER_FADE, MDE_ANIM_CH_LAUNCHER_SCALE, now_ms);
}

void mde_anim_launcher_dismiss(uint64_t now_ms)
{
    mde_anim_overlay_dismiss(MDE_ANIM_CH_LAUNCHER_FADE, MDE_ANIM_CH_LAUNCHER_SCALE, now_ms);
}
