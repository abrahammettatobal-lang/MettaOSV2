#ifndef METTA_RTL8822BU_H
#define METTA_RTL8822BU_H
#include <stdint.h>
typedef int (*usb_control_in_fn)(uint8_t, uint8_t, uint16_t, unsigned);
const char *rtl8822bu_probe(usb_control_in_fn request, uint8_t *buffer, unsigned capacity);
#endif
