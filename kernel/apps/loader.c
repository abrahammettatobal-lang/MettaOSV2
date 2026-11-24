/*
 * MettaOS — cargador de paquetes nativos (.mettaapp). SA-29
 *
 * Parsea manifiestos de texto, valida campos y prepara lanzamiento (stub
 * hasta ELF loader + scheduler + VFS — SA-09, SA-23, SA-24).
 */
#include "metta/types.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define METTAAPP_MAGIC_LINE     "mettaapp/1"
#define METTAAPP_CONTRACT_REV   1u

#define METTAAPP_ID_MAX         64u
#define METTAAPP_NAME_MAX       128u
#define METTAAPP_VERSION_MAX    32u
#define METTAAPP_PATH_MAX       256u

#define METTAAPP_REGISTRY_MAX   32u
#define METTAAPP_PARSE_BUF_MAX  4096u

typedef enum {
    METTAAPP_KIND_UNKNOWN = 0,
    METTAAPP_KIND_GUI,
    METTAAPP_KIND_CLI,
    METTAAPP_KIND_SERVICE
} mettaapp_kind_t;

typedef struct {
    char              id[METTAAPP_ID_MAX];
    char              name[METTAAPP_NAME_MAX];
    char              version[METTAAPP_VERSION_MAX];
    char              entry[METTAAPP_PATH_MAX];
    char              icon[METTAAPP_PATH_MAX];
    mettaapp_kind_t   kind;
    metta_u32         contract_rev;
} metta_app_manifest_t;

typedef struct {
    metta_app_manifest_t items[METTAAPP_REGISTRY_MAX];
    metta_u32            count;
    metta_u32            ready;
} metta_app_registry_t;

static metta_app_registry_t g_registry;

/* Manifiesto canónico embebido (formato de referencia SA-29). */
static const char g_mettaapp_example[] =
    "mettaapp/1\n"
    "id=notes\n"
    "name=Notes\n"
    "version=0.1.0\n"
    "entry=bin/notes.elf\n"
    "icon=icons/notes.png\n"
    "kind=gui\n";

static int mettaapp_char_ok_id(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

static mettaapp_kind_t mettaapp_kind_from_string(const char *s)
{
    if (s == NULL) {
        return METTAAPP_KIND_UNKNOWN;
    }
    if (strcmp(s, "gui") == 0) {
        return METTAAPP_KIND_GUI;
    }
    if (strcmp(s, "cli") == 0) {
        return METTAAPP_KIND_CLI;
    }
    if (strcmp(s, "service") == 0) {
        return METTAAPP_KIND_SERVICE;
    }
    return METTAAPP_KIND_UNKNOWN;
}

static metta_status_t mettaapp_copy_field(char *dst, size_t dst_sz, const char *src, size_t src_len)
{
    if (dst == NULL || dst_sz == 0u || src == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (src_len >= dst_sz) {
        return METTA_ERR_RANGE;
    }
    memcpy(dst, src, src_len);
    dst[src_len] = '\0';
    return METTA_OK;
}

static int mettaapp_entry_path_safe(const char *path)
{
    size_t i;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 0;
    }
    if (strstr(path, "..") != NULL) {
        return 0;
    }
    for (i = 0; path[i] != '\0'; i++) {
        char c = path[i];
        if (c == '\n' || c == '\r' || c == '\0') {
            return 0;
        }
    }
    return 1;
}

static metta_status_t mettaapp_validate_id(const char *id)
{
    size_t n;
    size_t i;

    if (id == NULL || id[0] == '\0') {
        return METTA_ERR_INVALID_ARG;
    }
    n = strlen(id);
    if (n >= METTAAPP_ID_MAX) {
        return METTA_ERR_RANGE;
    }
    for (i = 0; i < n; i++) {
        if (!mettaapp_char_ok_id(id[i])) {
            return METTA_ERR_INVALID_ARG;
        }
    }
    return METTA_OK;
}

static void mettaapp_manifest_clear(metta_app_manifest_t *m)
{
    if (m == NULL) {
        return;
    }
    memset(m, 0, sizeof(*m));
    m->kind = METTAAPP_KIND_UNKNOWN;
    m->contract_rev = METTAAPP_CONTRACT_REV;
}

static metta_status_t mettaapp_apply_kv(metta_app_manifest_t *m, const char *key, const char *val)
{
    metta_status_t st;

    if (m == NULL || key == NULL || val == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    if (strcmp(key, "id") == 0) {
        st = mettaapp_validate_id(val);
        if (st != METTA_OK) {
            return st;
        }
        return mettaapp_copy_field(m->id, sizeof(m->id), val, strlen(val));
    }
    if (strcmp(key, "name") == 0) {
        return mettaapp_copy_field(m->name, sizeof(m->name), val, strlen(val));
    }
    if (strcmp(key, "version") == 0) {
        return mettaapp_copy_field(m->version, sizeof(m->version), val, strlen(val));
    }
    if (strcmp(key, "entry") == 0) {
        if (!mettaapp_entry_path_safe(val)) {
            return METTA_ERR_INVALID_ARG;
        }
        return mettaapp_copy_field(m->entry, sizeof(m->entry), val, strlen(val));
    }
    if (strcmp(key, "icon") == 0) {
        if (!mettaapp_entry_path_safe(val)) {
            return METTA_ERR_INVALID_ARG;
        }
        return mettaapp_copy_field(m->icon, sizeof(m->icon), val, strlen(val));
    }
    if (strcmp(key, "kind") == 0) {
        mettaapp_kind_t k = mettaapp_kind_from_string(val);
        if (k == METTAAPP_KIND_UNKNOWN) {
            return METTA_ERR_INVALID_ARG;
        }
        m->kind = k;
        return METTA_OK;
    }

    /* Claves desconocidas se ignoran en rev 1 (forward compatibility). */
    return METTA_OK;
}

static const char *mettaapp_skip_line(const char *p)
{
    while (*p != '\0' && *p != '\n') {
        p++;
    }
    if (*p == '\n') {
        p++;
    }
    return p;
}

metta_status_t app_manifest_parse(const char *text, size_t text_len, metta_app_manifest_t *out)
{
    const char *p;
    const char *end;
    int        saw_magic;

    if (text == NULL || out == NULL || text_len == 0u || text_len > METTAAPP_PARSE_BUF_MAX) {
        return METTA_ERR_INVALID_ARG;
    }

    mettaapp_manifest_clear(out);
    p = text;
    end = text + text_len;
    saw_magic = 0;

    while (p < end && *p != '\0') {
        const char *line_start = p;
        const char *line_end;
        size_t      line_len;
        char        line_buf[512];
        char       *eq;
        char       *key;
        char       *val;

        p = mettaapp_skip_line(p);
        line_end = p;
        if (line_end > line_start && *(line_end - 1) == '\n') {
            line_end--;
        }
        line_len = (size_t)(line_end - line_start);
        if (line_len == 0u) {
            continue;
        }
        if (line_len >= sizeof(line_buf)) {
            return METTA_ERR_CORRUPT;
        }
        memcpy(line_buf, line_start, line_len);
        line_buf[line_len] = '\0';

        /* Comentarios */
        if (line_buf[0] == '#') {
            continue;
        }

        if (!saw_magic) {
            if (strcmp(line_buf, METTAAPP_MAGIC_LINE) != 0) {
                return METTA_ERR_CORRUPT;
            }
            saw_magic = 1;
            continue;
        }

        eq = strchr(line_buf, '=');
        if (eq == NULL) {
            return METTA_ERR_CORRUPT;
        }
        *eq = '\0';
        key = line_buf;
        val = eq + 1;

        {
            metta_status_t st = mettaapp_apply_kv(out, key, val);
            if (st != METTA_OK) {
                return st;
            }
        }
    }

    if (!saw_magic) {
        return METTA_ERR_CORRUPT;
    }
    if (out->id[0] == '\0' || out->name[0] == '\0' || out->version[0] == '\0'
        || out->entry[0] == '\0' || out->kind == METTAAPP_KIND_UNKNOWN) {
        return METTA_ERR_CORRUPT;
    }
    return METTA_OK;
}

metta_status_t app_loader_init(void)
{
    metta_status_t st;

    memset(&g_registry, 0, sizeof(g_registry));
    st = app_manifest_parse(g_mettaapp_example, strlen(g_mettaapp_example),
                            &g_registry.items[0]);
    if (st != METTA_OK) {
        return st;
    }
    g_registry.count = 1u;
    g_registry.ready = 1u;
    return METTA_OK;
}

void app_loader_shutdown(void)
{
    memset(&g_registry, 0, sizeof(g_registry));
}

metta_u32 app_loader_count(void)
{
    return g_registry.ready ? g_registry.count : 0u;
}

const metta_app_manifest_t *app_loader_get(metta_u32 index)
{
    if (!g_registry.ready || index >= g_registry.count) {
        return NULL;
    }
    return &g_registry.items[index];
}

metta_status_t app_loader_register_manifest(const metta_app_manifest_t *manifest)
{
    if (!g_registry.ready || manifest == NULL) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (g_registry.count >= METTAAPP_REGISTRY_MAX) {
        return METTA_ERR_NO_MEMORY;
    }
    memcpy(&g_registry.items[g_registry.count], manifest, sizeof(*manifest));
    g_registry.count++;
    return METTA_OK;
}

const metta_app_manifest_t *app_loader_find_by_id(const char *app_id)
{
    metta_u32 i;

    if (!g_registry.ready || app_id == NULL) {
        return NULL;
    }
    for (i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.items[i].id, app_id) == 0) {
            return &g_registry.items[i];
        }
    }
    return NULL;
}

metta_status_t app_launch(const char *app_id)
{
    const metta_app_manifest_t *m;

    if (!g_registry.ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    m = app_loader_find_by_id(app_id);
    if (m == NULL) {
        return METTA_ERR_NOT_FOUND;
    }

    /*
     * Futuro: resolver bundle dir vía VFS, app_elf_load(m->entry),
     * proc_create + MDE aviso de ventana si kind==gui.
     */
    (void)m;
    return METTA_ERR_BUSY;
}

metta_status_t app_manifest_parse_buffer(const char *text, metta_app_manifest_t *out)
{
    size_t n;

    if (text == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    n = strlen(text);
    if (n > METTAAPP_PARSE_BUF_MAX) {
        return METTA_ERR_RANGE;
    }
    return app_manifest_parse(text, n, out);
}