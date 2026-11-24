/*
 * MettaOS shell — sistema de archivos en RAM compartido por Archivos, Notas y Terminal.
 */
#include "ui_app.h"

#include <fs/vfs.h>
#include <log/klog.h>
#include "ata_data.h"
#include "../../kernel/fs/mfs/mfs_crc.h"

#define FS_SLOTS 64

/* The last slots describe the read-only AppImage medium, outside the MFS catalogue. */
static fs_node_t g_nodes[FS_NODE_CAP];
static char      g_data[FS_SLOTS][FS_FILE_CAP];
static uint8_t   g_slot_used[FS_SLOTS];
static int       g_home, g_desktop, g_docs, g_trash, g_apps;

/* MDE's file catalogue and text extents live on the mounted MFS volume.
 * Publish the next catalogue only after all changed extents have been flushed. */
typedef struct {
    uint32_t magic, generation;
    fs_node_t nodes[FS_MAX_NODES];
    uint32_t crc[FS_SLOTS];
    uint8_t bank[FS_SLOTS];
    uint32_t checksum;
} catalogue_t;
static catalogue_t catalogs[2];
static int active, dirty, write_error, locked;
static uint8_t changed[FS_SLOTS];
static uint64_t revision;

uint64_t fs_revision(void) { return revision; }

void fs_changed(int idx)
{
    revision++;
    dirty=1;
    if(idx>=0 && g_nodes[idx].slot>=0) changed[g_nodes[idx].slot]=1;
}

static void extent_name(char *path, unsigned slot, unsigned bank)
{
    ui_strlcpy(path,"/text000a",16);
    path[5]='0'+slot/100;
    path[6]='0'+slot/10%10;
    path[7]='0'+slot%10;
    path[8]='a'+bank;
}

static int disk_io(const char *path,void *buf,size_t len,int write)
{
    int fd;
    metta_status_t st=vfs_open(path,write ? VFS_O_CREAT|VFS_O_WRONLY : VFS_O_RDONLY,&fd);
    if(st!=METTA_OK) return st==METTA_ERR_NOT_FOUND ? 0 : -1;
    metta_ssize_t n=write ? vfs_write(fd,buf,len) : vfs_read(fd,buf,len);
    vfs_close(fd);
    return n==(metta_ssize_t)len ? 1 : -1;
}

static const char *const manifests[]={ "/desktop-a", "/desktop-b" };
int fs_read_glass(void)
{
    char value;
    return ata_data_mounted() && disk_io("/mde-glass", &value, 1, 0) == 1 && value == '1';
}

int fs_write_glass(int enabled)
{
    char value = enabled ? '1' : '0';
    return ata_data_mounted() && disk_io("/mde-glass", &value, 1, 1) == 1;
}

static int load_catalogue(catalogue_t *cat)
{
    uint8_t used[FS_SLOTS]={0};
    if(cat->magic!=0x3145444d || cat->checksum!=mfs_crc32(cat,offsetof(catalogue_t,checksum))) return 0;
    if(!cat->nodes[0].used || cat->nodes[0].type!=FS_DIR || cat->nodes[0].parent!=0) return 0;
    for(int i=0;i<FS_MAX_NODES;i++) {
        fs_node_t *n=cat->nodes+i;
        if(!n->used) continue;
        if(n->name[47] || n->parent<0 || n->parent>=FS_MAX_NODES ||
           !cat->nodes[n->parent].used || cat->nodes[n->parent].type!=FS_DIR ||
           n->type<FS_DIR || n->type>FS_APP || n->app<0 || n->app>=APP_COUNT) return 0;
        int parent=i;
        for(int depth=0;parent;depth++) {
            if(depth>=FS_MAX_NODES || parent<0 || parent>=FS_MAX_NODES) return 0;
            parent=cat->nodes[parent].parent;
        }
        if(n->type!=FS_TEXT) { if(n->slot!=-1) return 0; continue; }
        int slot=n->slot;
        if(slot<0 || slot>=FS_SLOTS || used[slot] || cat->bank[slot]>1 ||
           n->size<0 || n->size>=FS_FILE_CAP) return 0;
        used[slot]=1;
        char path[16];
        extent_name(path,slot,cat->bank[slot]);
        size_t bytes=(size_t)n->size+1;
        if(disk_io(path,g_data[slot],bytes,0)!=1 || g_data[slot][n->size] ||
           cat->crc[slot]!=mfs_crc32(g_data[slot],bytes)) return 0;
    }
    memcpy(g_nodes,cat->nodes,sizeof(cat->nodes));
    memcpy(g_slot_used,used,sizeof(used));
    g_home=fs_lookup(0,"/home/usuario");
    g_apps=fs_find(0,"Aplicaciones"); g_docs=fs_find(g_home,"Documentos");
    g_trash=fs_find(g_home,"Papelera"); g_desktop=fs_find(g_home,"Escritorio");
    return g_home>0 && g_apps>0 && g_docs>0 && g_trash>0 && g_desktop>0;
}

static int restore_desktop(void)
{
    if(!ata_data_mounted()) return 0;
    int a=disk_io(manifests[0],&catalogs[0],sizeof(catalogue_t),0);
    int b=disk_io(manifests[1],&catalogs[1],sizeof(catalogue_t),0);
    if(!a && !b) return 0;
    active=(a!=1 || (b==1 && catalogs[1].generation>catalogs[0].generation));
    for(int i=0;i<2;i++,active^=1) {
        if((active ? b : a)==1 && load_catalogue(&catalogs[active])) {
            KLOG_INFO("[mde] restored desktop from MFS\n"); return 1;
        }
    }
    locked=1; /* Preserve the disk for recovery. */
    return 0;
}

void fs_sync(void)
{
    if(!dirty || locked || !ata_data_mounted()) return;
    catalogue_t *old=&catalogs[active], *next=&catalogs[active^1];
    *next=*old;
    next->magic=0x3145444d;
    next->generation=old->generation+1;
    memcpy(next->nodes,g_nodes,sizeof(next->nodes));
    for(int i=0;i<FS_SLOTS;i++) {
        if(!changed[i] || !g_slot_used[i]) continue;
        size_t len=strlen(g_data[i])+1;
        next->bank[i]=1-old->bank[i];
        next->crc[i]=mfs_crc32(g_data[i],len);
        char path[16];
        extent_name(path,i,next->bank[i]);
        if(disk_io(path,g_data[i],len,1)!=1) goto error;
    }
    next->checksum=mfs_crc32(next,offsetof(catalogue_t,checksum));
    if(disk_io(manifests[active^1],next,sizeof(*next),1)!=1) goto error;
    active^=1; dirty=write_error=0;
    memset(changed,0,sizeof(changed));
    return;
error:
    if(!write_error) KLOG_WARN("[mde] write to MFS failed\n");
    write_error=1;
}

const char *fs_disk_status(void)
{
    if(locked) return "Error al leer el disco";
    if(write_error) return "Error al guardar en MFS";
    return ata_data_mounted() ? "Disco MFS" : "Temporal (RAM)";
}

fs_node_t *fs_node(int idx)
{
    if (idx < 0 || idx >= FS_NODE_CAP || !g_nodes[idx].used)
        return NULL;
    return &g_nodes[idx];
}

int fs_root(void) { return 0; }
int fs_home(void) { return g_home; }
int fs_desktop(void) { return g_desktop; }
int fs_docs(void) { return g_docs; }
int fs_trash(void) { return g_trash; }
int fs_apps_dir(void) { return g_apps; }

void fs_import_image(unsigned index, const char *name)
{
    if (index >= FS_IMAGE_LIMIT || !name || !*name) return;
    fs_node_t *n = &g_nodes[FS_MAX_NODES + index];
    *n = (fs_node_t){ .used = 1, .parent = g_apps, .type = FS_APPIMAGE, .slot = -1 };
    ui_strlcpy(n->name, name, sizeof(n->name));
    int other = fs_find(g_apps, n->name);
    if (strlen(name) >= sizeof(n->name) || other != (int)(FS_MAX_NODES + index)) {
        char number[12];
        unsigned end = ui_min((int)strlen(name), 30);
        while (end && ((unsigned char)name[end] & 0xc0u) == 0x80u) end--;
        n->name[end] = 0;
        ui_itoa(index + 1, number);
        ui_strlcat(n->name, "~", sizeof(n->name));
        ui_strlcat(n->name, number, sizeof(n->name));
        ui_strlcat(n->name, ".AppImage", sizeof(n->name));
    }
}

int fs_find(int parent, const char *name)
{
    int i;

    for (i = 0; i < FS_NODE_CAP; i++) {
        if (g_nodes[i].used && g_nodes[i].parent == parent && i != parent &&
            ui_streq(g_nodes[i].name, name))
            return i;
    }
    return -1;
}

int fs_create(int parent, const char *name, int type)
{
    int i, s;

    if (!fs_node(parent) || g_nodes[parent].type != FS_DIR || !name[0])
        return -1;
    if (fs_find(parent, name) >= 0)
        return -1;
    for (i = 1; i < FS_MAX_NODES; i++) {
        if (!g_nodes[i].used)
            break;
    }
    if (i >= FS_MAX_NODES)
        return -1;

    memset(&g_nodes[i], 0, sizeof(g_nodes[i]));
    g_nodes[i].slot = -1;
    if (type == FS_TEXT) {
        for (s = 0; s < FS_SLOTS && g_slot_used[s]; s++)
            ;
        if (s >= FS_SLOTS)
            return -1;
        g_slot_used[s] = 1;
        g_data[s][0] = '\0';
        g_nodes[i].slot = s;
    }
    g_nodes[i].used = 1;
    g_nodes[i].parent = parent;
    g_nodes[i].type = type;
    ui_strlcpy(g_nodes[i].name, name, sizeof(g_nodes[i].name));
    fs_changed(i);
    return i;
}

int fs_remove(int idx)
{
    int i;

    if (idx <= 0 || idx >= FS_MAX_NODES || !fs_node(idx))
        return -1;
    if (idx == g_nodes[g_home].parent || idx == g_home || idx == g_trash || idx == g_docs || idx == g_desktop || idx == g_apps)
        return -1;
    for (i = 0; i < FS_MAX_NODES; i++) {
        if (g_nodes[i].used && g_nodes[i].parent == idx && i != idx)
            fs_remove(i);
    }
    if (g_nodes[idx].slot >= 0)
        g_slot_used[g_nodes[idx].slot] = 0;
    g_nodes[idx].used = 0;
    dirty=1;
    return 0;
}

int fs_move(int idx, int new_parent)
{
    char name[48];

    if (idx <= 0 || idx >= FS_MAX_NODES || !fs_node(idx) || !fs_node(new_parent) || g_nodes[new_parent].type != FS_DIR)
        return -1;
    if (idx == g_home || idx == g_desktop || idx == g_docs || idx == g_apps || idx == g_trash || idx == g_nodes[g_home].parent) return -1;
    for (int p=new_parent;p;p=g_nodes[p].parent) if(p==idx) return -1;
    if (fs_find(new_parent, g_nodes[idx].name) >= 0) {
        fs_unique_name(new_parent, g_nodes[idx].name, "", name, sizeof(name));
        ui_strlcpy(g_nodes[idx].name, name, sizeof(g_nodes[idx].name));
    }
    g_nodes[idx].parent = new_parent;
    dirty=1;
    return 0;
}

int fs_unique_name(int parent, const char *base, const char *ext, char *out, size_t cap)
{
    int n;
    char num[12];

    ui_strlcpy(out, base, cap);
    ui_strlcat(out, ext, cap);
    for (n = 2; fs_find(parent, out) >= 0 && n < 999; n++) {
        ui_strlcpy(out, base, cap);
        ui_strlcat(out, " ", cap);
        ui_itoa(n, num);
        ui_strlcat(out, num, cap);
        ui_strlcat(out, ext, cap);
    }
    return 0;
}

void fs_path(int idx, char *out, size_t cap)
{
    int chain[24];
    int n = 0, i;

    out[0] = '\0';
    while (idx > 0 && n < 24 && fs_node(idx)) {
        chain[n++] = idx;
        idx = g_nodes[idx].parent;
    }
    if (n == 0) {
        ui_strlcpy(out, "/", cap);
        return;
    }
    for (i = n - 1; i >= 0; i--) {
        ui_strlcat(out, "/", cap);
        ui_strlcat(out, g_nodes[chain[i]].name, cap);
    }
}

int fs_lookup(int cwd, const char *path)
{
    char part[48];
    int cur = cwd, i = 0, n;

    if (!path || !path[0])
        return cwd;
    if (path[0] == '/') {
        cur = 0;
        i = 1;
    } else if (path[0] == '~') {
        cur = g_home;
        i = 1;
        if (path[1] == '/')
            i = 2;
    }
    while (path[i]) {
        n = 0;
        while (path[i] && path[i] != '/' && n < 47)
            part[n++] = path[i++];
        part[n] = '\0';
        if (path[i] == '/')
            i++;
        if (n == 0 || ui_streq(part, "."))
            continue;
        if (ui_streq(part, "..")) {
            if (cur > 0)
                cur = g_nodes[cur].parent;
            continue;
        }
        cur = fs_find(cur, part);
        if (cur < 0)
            return -1;
    }
    return cur;
}

char *fs_data(int idx)
{
    fs_node_t *n = fs_node(idx);

    if (!n || n->slot < 0)
        return NULL;
    return g_data[n->slot];
}

void fs_set_text(int idx, const char *text)
{
    char *d = fs_data(idx);

    if (!d)
        return;
    ui_strlcpy(d, text, FS_FILE_CAP);
    g_nodes[idx].size = (int)strlen(d);
    fs_changed(idx);
}

int fs_child_next(int parent, int after)
{
    int i;

    for (i = after + 1; i < FS_NODE_CAP; i++) {
        if (g_nodes[i].used && g_nodes[i].parent == parent && i != parent)
            return i;
    }
    return -1;
}

int fs_count_children(int parent)
{
    int n = 0, i = -1;

    while ((i = fs_child_next(parent, i)) >= 0)
        n++;
    return n;
}

int fs_icon(int idx)
{
    fs_node_t *n = fs_node(idx);

    if (!n)
        return UI_ICON_DOC;
    switch (n->type) {
    case FS_DIR:
        return idx == g_trash ? UI_ICON_TRASH : UI_ICON_FOLDER;
    case FS_APPIMAGE:
        return UI_ICON_APPIMAGE;
    case FS_APP:
        return ui_apps[n->app]->icon;
    default:
        return UI_ICON_DOC;
    }
}

void fs_open(int idx)
{
    fs_node_t *n = fs_node(idx);
    char path[160];

    if (!n)
        return;
    switch (n->type) {
    case FS_DIR:
        fs_path(idx, path, sizeof(path));
        ui_open_app(APP_FILES, path);
        break;
    case FS_TEXT:
        fs_path(idx, path, sizeof(path));
        ui_open_app(APP_NOTES, path);
        break;
    case FS_APPIMAGE:
        if (idx >= FS_MAX_NODES) ui_open_app(APP_LINUX, n->name);
        else {
            fs_path(idx,path,sizeof(path));
            ui_open_app(APP_LINUX,path);
        }
        break;
    case FS_APP:
        ui_open_app(n->app, NULL);
        break;
    default:
        break;
    }
}

static int mk(int parent, const char *name, int type)
{
    return fs_create(parent, name, type);
}

static const char k_welcome[] =
    "Bienvenido a MettaOS\n"
    "\n"
    "Este es tu escritorio. Todo lo que ves funciona:\n"
    "\n"
    "• Dock (abajo): abre Archivos, Notas, Terminal, Calculadora y Ajustes.\n"
    "• Ventanas: arrastra desde la barra superior, cambia el tamaño desde los bordes,\n"
    "  y usa los botones rojo (cerrar), amarillo (minimizar) y verde (zoom).\n"
    "• Notas: escribe aquí mismo; se guarda solo en Documentos.\n"
    "• Terminal: escribe \"help\" para ver los comandos (ls, cd, cat, mkdir, open...).\n"
    "• Búsqueda: Ctrl+Espacio abre la búsqueda para lanzar apps y archivos.\n"
    "• Ajustes: cambia el fondo de pantalla y la distribución del teclado.\n"
    "\n"
    "Atajos: Ctrl+N nuevo, Ctrl+W cerrar ventana, Ctrl+M minimizar,\n"
    "Ctrl+Q salir de la app, Alt+Tab cambiar de ventana.\n";

static const char k_shortcuts[] =
    "Atajos de teclado de MettaOS\n"
    "\n"
    "Ctrl+Espacio   Búsqueda\n"
    "Ctrl+N         Nueva ventana / nueva nota\n"
    "Ctrl+W         Cerrar ventana\n"
    "Ctrl+M         Minimizar\n"
    "Ctrl+Q         Salir de la app\n"
    "Alt+Tab        Siguiente ventana\n"
    "Supr           Mover a la Papelera (Archivos)\n"
    "Doble clic     Abrir / zoom en la barra de título\n";

static void ensure_firefox_node(void)
{
    if (g_docs <= 0 || fs_find(g_docs, "Firefox.AppImage") >= 0)
        return;
    int fd;
    if (vfs_open("/home/usuario/Documentos/Firefox.AppImage", VFS_O_RDONLY, &fd) != METTA_OK)
        return;
    vfs_close(fd);
    (void)fs_create(g_docs, "Firefox.AppImage", FS_APPIMAGE);
}

void fs_init(void)
{
    int usr, f;

    if (restore_desktop()) {
        /* Old catalogues shipped a zero-byte shortcut to the native calculator. */
        for (int i = 1; i < FS_MAX_NODES; i++)
            if (g_nodes[i].used && g_nodes[i].type == FS_APPIMAGE && g_nodes[i].app == APP_CALC &&
                !g_nodes[i].size && ui_streq(g_nodes[i].name, "Calculator.AppImage"))
                fs_remove(i);
        ensure_firefox_node();
        fs_sync();
        return;
    }
    memset(g_nodes, 0, sizeof(g_nodes));
    memset(g_slot_used, 0, sizeof(g_slot_used));
    g_nodes[0].used = 1;
    g_nodes[0].type = FS_DIR;
    g_nodes[0].parent = 0;
    g_nodes[0].slot = -1;
    ui_strlcpy(g_nodes[0].name, "", sizeof(g_nodes[0].name));

    g_apps = mk(0, "Aplicaciones", FS_DIR);
    usr = mk(mk(0, "home", FS_DIR), "usuario", FS_DIR);
    g_home = usr;
    g_desktop = mk(usr, "Escritorio", FS_DIR);
    g_docs = mk(usr, "Documentos", FS_DIR);
    mk(usr, "Descargas", FS_DIR);
    mk(usr, "Imágenes", FS_DIR);
    mk(usr, "Música", FS_DIR);
    g_trash = mk(usr, "Papelera", FS_DIR);

    f = mk(g_apps, "Archivos", FS_APP);
    g_nodes[f].app = APP_FILES;
    f = mk(g_apps, "Notas", FS_APP);
    g_nodes[f].app = APP_NOTES;
    f = mk(g_apps, "Terminal", FS_APP);
    g_nodes[f].app = APP_TERMINAL;
    f = mk(g_apps, "Calculadora", FS_APP);
    g_nodes[f].app = APP_CALC;
    f = mk(g_apps, "Ajustes", FS_APP);
    g_nodes[f].app = APP_SETTINGS;

    fs_set_text(mk(g_docs, "Bienvenida.txt", FS_TEXT), k_welcome);
    fs_set_text(mk(g_docs, "Atajos.txt", FS_TEXT), k_shortcuts);
    fs_set_text(mk(g_docs, "Ideas.txt", FS_TEXT),
                "Ideas\n\n- Probar la calculadora\n- Cambiar el fondo en Ajustes\n");
    fs_set_text(mk(g_desktop, "Léeme.txt", FS_TEXT), k_welcome);
    ensure_firefox_node();
    fs_sync();
}
