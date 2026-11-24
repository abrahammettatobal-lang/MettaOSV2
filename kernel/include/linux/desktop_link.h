#ifndef METTA_DESKTOP_LINK_H
#define METTA_DESKTOP_LINK_H

#include <stdint.h>

/* Private virtio console used between the two QEMU guests. */
int desktop_link_init(void);
int desktop_link_send(char type, unsigned slot, const void *data, unsigned length);
void desktop_link_poll(void (*message)(char, unsigned, const uint8_t *, unsigned));

#endif
