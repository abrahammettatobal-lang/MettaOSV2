/*
 * MettaOS — puente X11 sobre AF_UNIX (stub SA-34).
 * Sin ventanas falsas: solo registra XIDs tras tráfico real del cliente.
 */
#include "linux/runtime.h"

#include <log/printk.h>
#include <metta/types.h>

#include <stddef.h>
#include <stdint.h>

#define X11_BRIDGE_REV              1u
#define X11_BRIDGE_MAX_CONN         8u
#define X11_BRIDGE_MAX_WINDOWS      64u
#define X11_BRIDGE_SUN_PATH_MAX     108u

/* ABI Linux — sockaddr_un */
#define LINUX_AF_UNIX               1

/* X11 core requests (mínimo para stub) */
#define X11_OP_CREATE_WINDOW        1u
#define X11_OP_MAP_WINDOW           8u
#define X11_OP_DESTROY_WINDOW       18u

typedef enum {
    X11_CONN_IDLE = 0,
    X11_CONN_HANDSHAKE,
    X11_CONN_READY
} x11_conn_phase_t;

typedef struct {
    int              in_use;
    metta_u32        id;
    linux_pid_t      pid;
    char             sun_path[X11_BRIDGE_SUN_PATH_MAX];
    x11_conn_phase_t phase;
} x11_bridge_conn_t;

typedef struct {
    int         in_use;
    metta_u32   xid;
    metta_u32   conn_id;
    linux_pid_t owner;
    metta_u32   width;
    metta_u32   height;
    int         mapped;
} x11_bridge_window_t;

static int              g_x11_ready;
static x11_bridge_conn_t g_conns[X11_BRIDGE_MAX_CONN];
static x11_bridge_window_t g_windows[X11_BRIDGE_MAX_WINDOWS];
static metta_u32        g_next_conn_id = 1u;

static int x11_path_is_display_socket(const char *path)
{
    static const char marker[] = ".X11-unix/X";
    size_t            len;
    size_t            i;

    if (path == NULL) {
        return 0;
    }
    len = 0;
    while (path[len] != '\0' && len < X11_BRIDGE_SUN_PATH_MAX) {
        len++;
    }
    if (len == 0u) {
        return 0;
    }

    for (i = 0; i + sizeof(marker) - 1u <= len; i++) {
        size_t j;
        int    ok = 1;
        for (j = 0; j < sizeof(marker) - 1u; j++) {
            if (path[i + j] != marker[j]) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            return 1;
        }
    }
    return 0;
}

static metta_u32 x11_read_be32(const metta_u8 *p)
{
    return ((metta_u32)p[0] << 24) | ((metta_u32)p[1] << 16)
         | ((metta_u32)p[2] << 8) | (metta_u32)p[3];
}

static metta_u16 x11_read_be16(const metta_u8 *p)
{
    return (metta_u16)(((metta_u16)p[0] << 8) | (metta_u16)p[1]);
}

static x11_bridge_conn_t *x11_conn_by_id(metta_u32 conn_id)
{
    metta_u32 i;

    for (i = 0; i < X11_BRIDGE_MAX_CONN; i++) {
        if (g_conns[i].in_use && g_conns[i].id == conn_id) {
            return &g_conns[i];
        }
    }
    return NULL;
}

static x11_bridge_window_t *x11_window_by_xid(metta_u32 xid)
{
    metta_u32 i;

    for (i = 0; i < X11_BRIDGE_MAX_WINDOWS; i++) {
        if (g_windows[i].in_use && g_windows[i].xid == xid) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static metta_status_t x11_window_upsert(metta_u32 conn_id, linux_pid_t owner,
                                        metta_u32 xid, metta_u32 w, metta_u32 h,
                                        int mapped)
{
    x11_bridge_window_t *slot;
    metta_u32            i;

    slot = x11_window_by_xid(xid);
    if (slot != NULL) {
        slot->width  = w;
        slot->height = h;
        if (mapped) {
            slot->mapped = 1;
        }
        return METTA_OK;
    }

    for (i = 0; i < X11_BRIDGE_MAX_WINDOWS; i++) {
        if (!g_windows[i].in_use) {
            g_windows[i].in_use = 1;
            g_windows[i].xid    = xid;
            g_windows[i].conn_id = conn_id;
            g_windows[i].owner  = owner;
            g_windows[i].width  = w;
            g_windows[i].height = h;
            g_windows[i].mapped = mapped ? 1 : 0;
            return METTA_OK;
        }
    }
    return METTA_ERR_NO_MEMORY;
}

static void x11_window_drop(metta_u32 xid)
{
    x11_bridge_window_t *slot = x11_window_by_xid(xid);

    if (slot == NULL) {
        return;
    }
    slot->in_use = 0;
    slot->xid    = 0;
    slot->mapped = 0;
    slot->width  = 0;
    slot->height = 0;
}

static void x11_parse_client_requests(x11_bridge_conn_t *conn,
                                      const metta_u8 *data, metta_u64 len)
{
    metta_u64 off = 0;

    if (conn == NULL || data == NULL) {
        return;
    }

    while (off + 4u <= len) {
        metta_u8 op = data[off];

        if (op == X11_OP_CREATE_WINDOW && off + 32u <= len) {
            metta_u32 wid = (metta_u32)x11_read_be16(data + off + 4u);
            metta_u32 w   = (metta_u32)x11_read_be16(data + off + 16u);
            metta_u32 h   = (metta_u32)x11_read_be16(data + off + 18u);

            if (wid != 0u && w > 0u && h > 0u) {
                (void)x11_window_upsert(conn->id, conn->pid, wid, w, h, 0);
            }
            off += 32u;
            continue;
        }

        if (op == X11_OP_MAP_WINDOW && off + 8u <= len) {
            metta_u32 wid = x11_read_be32(data + off + 4u);
            x11_bridge_window_t *win = x11_window_by_xid(wid);

            if (win != NULL) {
                win->mapped = 1;
            }
            off += 8u;
            continue;
        }

        if (op == X11_OP_DESTROY_WINDOW && off + 8u <= len) {
            metta_u32 wid = x11_read_be32(data + off + 4u);
            x11_window_drop(wid);
            off += 8u;
            continue;
        }

        /* Stub: ignorar bytes no reconocidos sin inventar ventanas. */
        off += 1u;
        if (off >= len) {
            break;
        }
    }
}

metta_status_t x11_bridge_init(void)
{
    metta_u32 i;

    if (g_x11_ready) {
        return METTA_OK;
    }

    for (i = 0; i < X11_BRIDGE_MAX_CONN; i++) {
        g_conns[i].in_use = 0;
    }
    for (i = 0; i < X11_BRIDGE_MAX_WINDOWS; i++) {
        g_windows[i].in_use = 0;
    }

    g_x11_ready = 1;
    printk("[x11] bridge stub rev=%u (AF_UNIX, no fake wins)\n", X11_BRIDGE_REV);
    return METTA_OK;
}

bool x11_bridge_ready(void)
{
    return g_x11_ready != 0;
}

void x11_bridge_shutdown(void)
{
    metta_u32 i;

    for (i = 0; i < X11_BRIDGE_MAX_CONN; i++) {
        g_conns[i].in_use = 0;
    }
    for (i = 0; i < X11_BRIDGE_MAX_WINDOWS; i++) {
        g_windows[i].in_use = 0;
    }
    g_x11_ready = 0;
}

/*
 * Registra una conexión AF_UNIX hacia el display X11 del stub.
 * No crea ventanas MDE ni XIDs sintéticos.
 */
metta_status_t x11_bridge_af_unix_accept(linux_pid_t pid, int sock_family,
                                         const char *sun_path,
                                         metta_u32 *conn_id_out)
{
    metta_u32 i;

    if (!g_x11_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (conn_id_out == NULL || sun_path == NULL || pid == LINUX_PID_INVALID) {
        return METTA_ERR_INVALID_ARG;
    }
    if (sock_family != LINUX_AF_UNIX) {
        return METTA_ERR_INVALID_ARG;
    }
    if (!x11_path_is_display_socket(sun_path)) {
        return METTA_ERR_INVALID_ARG;
    }

    for (i = 0; i < X11_BRIDGE_MAX_CONN; i++) {
        if (g_conns[i].in_use) {
            continue;
        }
        g_conns[i].in_use = 1;
        g_conns[i].id     = g_next_conn_id++;
        g_conns[i].pid    = pid;
        g_conns[i].phase  = X11_CONN_HANDSHAKE;
        for (metta_u32 j = 0; j < X11_BRIDGE_SUN_PATH_MAX; j++) {
            g_conns[i].sun_path[j] = sun_path[j];
            if (sun_path[j] == '\0') {
                break;
            }
        }
        g_conns[i].sun_path[X11_BRIDGE_SUN_PATH_MAX - 1u] = '\0';
        *conn_id_out = g_conns[i].id;
        return METTA_OK;
    }
    return METTA_ERR_NO_MEMORY;
}

metta_status_t x11_bridge_af_unix_close(metta_u32 conn_id)
{
    x11_bridge_conn_t *conn = x11_conn_by_id(conn_id);
    metta_u32          i;

    if (!g_x11_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (conn == NULL) {
        return METTA_ERR_NOT_FOUND;
    }

    for (i = 0; i < X11_BRIDGE_MAX_WINDOWS; i++) {
        if (g_windows[i].in_use && g_windows[i].conn_id == conn_id) {
            g_windows[i].in_use = 0;
        }
    }

    conn->in_use = 0;
    return METTA_OK;
}

/*
 * Bytes X11 desde el cliente Linux. Solo ventanas explícitas en el protocolo.
 */
metta_status_t x11_bridge_feed(metta_u32 conn_id, const void *data, metta_u64 len)
{
    x11_bridge_conn_t *conn = x11_conn_by_id(conn_id);

    if (!g_x11_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (conn == NULL || data == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (len == 0u) {
        return METTA_OK;
    }

    if (conn->phase == X11_CONN_HANDSHAKE) {
        conn->phase = X11_CONN_READY;
    }

    x11_parse_client_requests(conn, (const metta_u8 *)data, len);
    return METTA_OK;
}

int x11_bridge_window_query(metta_u32 xid, metta_u32 *width_out,
                            metta_u32 *height_out, int *mapped_out)
{
    x11_bridge_window_t *win = x11_window_by_xid(xid);

    if (!g_x11_ready) {
        return 0;
    }
    if (win == NULL || !win->mapped) {
        return 0;
    }
    if (width_out != NULL) {
        *width_out = win->width;
    }
    if (height_out != NULL) {
        *height_out = win->height;
    }
    if (mapped_out != NULL) {
        *mapped_out = win->mapped;
    }
    return 1;
}

metta_u32 x11_bridge_window_count(void)
{
    metta_u32 i;
    metta_u32 n = 0;

    for (i = 0; i < X11_BRIDGE_MAX_WINDOWS; i++) {
        if (g_windows[i].in_use && g_windows[i].mapped) {
            n++;
        }
    }
    return n;
}