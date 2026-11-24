/*
 * MettaOS MDE — incrustación de ventanas Linux vía puente X11 (SA-34).
 * Sin ventanas falsas: solo crea shells MDE si el puente reporta un XID mapeado.
 */
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MDE_EMBED_OK = 0,
    MDE_EMBED_ERR_ARG,
    MDE_EMBED_ERR_NO_BRIDGE,
    MDE_EMBED_ERR_NOT_FOUND,
    MDE_EMBED_ERR_LIMIT
} mde_embed_status_t;

#define MDE_LINUX_EMBED_MAX 32u

/* Contrato cruzado con kernel/linux/x11_bridge.c (enlace futuro). */
extern int x11_bridge_window_query(uint32_t xid, uint32_t *width_out,
                                   uint32_t *height_out, int *mapped_out);

typedef struct {
    int       active;
    uint32_t  linux_pid;
    uint32_t  x11_xid;
    uint32_t  width;
    uint32_t  height;
    uintptr_t mde_surface_id; /* 0 hasta que el compositor asigne superficie real */
} mde_linux_embed_slot_t;

static mde_linux_embed_slot_t g_embeds[MDE_LINUX_EMBED_MAX];

static mde_linux_embed_slot_t *mde_embed_find(uint32_t xid)
{
    uint32_t i;

    for (i = 0; i < MDE_LINUX_EMBED_MAX; i++) {
        if (g_embeds[i].active && g_embeds[i].x11_xid == xid) {
            return &g_embeds[i];
        }
    }
    return NULL;
}

mde_embed_status_t mde_linux_embed_init(void)
{
    uint32_t i;

    for (i = 0; i < MDE_LINUX_EMBED_MAX; i++) {
        g_embeds[i].active = 0;
    }
    return MDE_EMBED_OK;
}

/*
 * Intenta enlazar un XID Linux con una ventana MDE.
 * Falla si el puente no tiene ventana mapeada (no placeholder).
 */
mde_embed_status_t mde_linux_embed_attach(uint32_t linux_pid, uint32_t x11_xid,
                                          uintptr_t *surface_id_out)
{
    uint32_t w = 0;
    uint32_t h = 0;
    int      mapped = 0;
    uint32_t i;

    if (linux_pid == 0u || x11_xid == 0u) {
        return MDE_EMBED_ERR_ARG;
    }

    if (!x11_bridge_window_query(x11_xid, &w, &h, &mapped)) {
        return MDE_EMBED_ERR_NOT_FOUND;
    }
    if (!mapped || w == 0u || h == 0u) {
        return MDE_EMBED_ERR_NOT_FOUND;
    }

    if (mde_embed_find(x11_xid) != NULL) {
        return MDE_EMBED_OK;
    }

    for (i = 0; i < MDE_LINUX_EMBED_MAX; i++) {
        if (!g_embeds[i].active) {
            g_embeds[i].active          = 1;
            g_embeds[i].linux_pid       = linux_pid;
            g_embeds[i].x11_xid         = x11_xid;
            g_embeds[i].width           = w;
            g_embeds[i].height          = h;
            g_embeds[i].mde_surface_id  = 0u;
            if (surface_id_out != NULL) {
                *surface_id_out = 0u;
            }
            return MDE_EMBED_OK;
        }
    }
    return MDE_EMBED_ERR_LIMIT;
}

mde_embed_status_t mde_linux_embed_sync_geometry(uint32_t x11_xid)
{
    mde_linux_embed_slot_t *slot;
    uint32_t                w;
    uint32_t                h;
    int                     mapped;

    slot = mde_embed_find(x11_xid);
    if (slot == NULL) {
        return MDE_EMBED_ERR_NOT_FOUND;
    }

    if (!x11_bridge_window_query(x11_xid, &w, &h, &mapped)) {
        slot->active = 0;
        return MDE_EMBED_ERR_NO_BRIDGE;
    }
    if (!mapped) {
        slot->active = 0;
        return MDE_EMBED_ERR_NOT_FOUND;
    }

    slot->width  = w;
    slot->height = h;
    return MDE_EMBED_OK;
}

void mde_linux_embed_detach(uint32_t x11_xid)
{
    mde_linux_embed_slot_t *slot = mde_embed_find(x11_xid);

    if (slot != NULL) {
        slot->active = 0;
    }
}

void mde_linux_embed_detach_all_for_pid(uint32_t linux_pid)
{
    uint32_t i;

    if (linux_pid == 0u) {
        return;
    }
    for (i = 0; i < MDE_LINUX_EMBED_MAX; i++) {
        if (g_embeds[i].active && g_embeds[i].linux_pid == linux_pid) {
            g_embeds[i].active = 0;
        }
    }
}

uint32_t mde_linux_embed_active_count(void)
{
    uint32_t i;
    uint32_t n = 0;

    for (i = 0; i < MDE_LINUX_EMBED_MAX; i++) {
        if (g_embeds[i].active) {
            n++;
        }
    }
    return n;
}