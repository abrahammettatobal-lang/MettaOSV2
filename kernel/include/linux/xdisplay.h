#ifndef METTA_LINUX_XDISPLAY_H
#define METTA_LINUX_XDISPLAY_H

/*
 * Pantalla X11 mínima (DISPLAY=:0) para que Firefox dibuje en la ventana.
 * Sin extensión RENDER: Cairo cae a PutImage/CopyArea y esos píxeles se pintan.
 */
#include <stdint.h>

#define LINUX_XDISPLAY_W 800
#define LINUX_XDISPLAY_H 480

typedef void (*linux_xdisplay_paint_fn)(const uint32_t *px, int w, int h);

void linux_xdisplay_reset(void);
void linux_xdisplay_on_paint(linux_xdisplay_paint_fn fn);
int  linux_xdisplay_is_path(const void *addr, uint64_t len);
void linux_xdisplay_attach(int fd);
int  linux_xdisplay_owned(int fd);
int64_t linux_xdisplay_read(int fd, void *buf, uint64_t count);
int64_t linux_xdisplay_write(int fd, const void *buf, uint64_t count);
int  linux_xdisplay_poll_in(int fd);

#endif /* METTA_LINUX_XDISPLAY_H */
