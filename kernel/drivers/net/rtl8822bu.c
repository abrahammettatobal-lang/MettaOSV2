#include <rtl8822bu.h>
#include <log/klog.h>

const char *rtl8822bu_probe(usb_control_in_fn request, uint8_t *buffer, unsigned capacity)
{
    if (capacity < 9 || !request(0x80, 6, 0x0200, 9) || buffer[0] != 9 || buffer[1] != 2)
        return "Archer: error de descriptor USB";
    unsigned length = buffer[2] | (buffer[3] << 8);
    if (length < 9 || length > capacity || !request(0x80, 6, 0x0200, length))
        return "Archer: configuracion USB incompleta";
    unsigned config = buffer[5], in = 0, out = 0;
    int vendor = 0;
    for (unsigned pos = 9; pos + 2 <= length;) {
        const uint8_t *d = buffer + pos;
        if (d[0] < 2 || d[0] > length - pos) return "Archer: descriptor USB invalido";
        if (d[1] == 4 && d[0] >= 9) {
            if (in && out) break;
            in = out = 0;
            vendor = d[3] == 0 && d[5] == 255 && d[6] == 255 && d[7] == 255;
        } else if (vendor && d[1] == 5 && d[0] >= 7 && (d[3] & 3) == 2 && (d[2] & 15)) {
            if (d[2] & 128) in = d[2];
            else out = d[2];
        }
        pos += d[0];
    }
    if (!in || !out || !config) return "Archer: interfaz USB incompatible";
    KLOG_INFO("[wifi] Archer bulk IN=%x OUT=%x\n", in, out);
    if (!request(0, 9, config, 0) || !request(0xc0, 5, 0x00f0, 4))
        return "Archer: chip sin respuesta";
    uint32_t revision = buffer[0] | ((uint32_t)buffer[1] << 8) |
                        ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
    KLOG_INFO("[wifi] RTL8822BU SYS_CFG1=%x\n", revision);
    return "RTL8822BU identificado; radio sin inicializar";
}
