/* RTL8139 in the QEMU PC profile. Kernel static storage is identity mapped. */
#include <interrupt/ports.h>
#include <log/klog.h>
#include <network.h>

#define RX_SIZE 8192u
static uint8_t rx[RX_SIZE + 16 + 2048] __attribute__((aligned(256)));
static uint8_t tx[4][1536] __attribute__((aligned(256)));
static uint8_t mac[6];
static const uint8_t own_ip[4] = {10, 0, 2, 15}, router[4] = {10, 0, 2, 2};
static unsigned cursor, slot;
static uint16_t base;
static enum net_state state;
static uint64_t next_probe, last_reply;
static inline void wr32(uint16_t p, uint32_t v)
{
    __asm__ volatile("outl %0,%1" ::"a"(v), "Nd"(p) : "memory");
}
static inline uint32_t rd32(uint16_t p)
{
    uint32_t v;
    __asm__ volatile("inl %1,%0" : "=a"(v) : "Nd"(p) : "memory");
    return v;
}
static inline uint16_t rd16(uint16_t p)
{
    uint16_t v;
    __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p) : "memory");
    return v;
}
static void copy(uint8_t *d, const uint8_t *s, unsigned n)
{
    while (n--)
        *d++ = *s++;
}
static int equal(const uint8_t *a, const uint8_t *b, unsigned n)
{
    while (n--)
        if (*a++ != *b++)
            return 0;
    return 1;
}
static void put16(uint8_t *p, unsigned n)
{
    p[0] = n >> 8;
    p[1] = n;
}
static unsigned get16(const uint8_t *p) { return ((unsigned)p[0] << 8) | p[1]; }

static int reset_nic(void)
{
    outb(base + 0x37, 0x10);
    unsigned timeout = 10000;
    while ((inb(base + 0x37) & 0x10) && --timeout) {
    }
    if (!timeout)
        return 0;
    cursor = slot = 0;
    wr32(base + 0x30, (uintptr_t)rx);
    outw(base + 0x3c, 0);
    outb(base + 0x37, 0x0c);
    wr32(base + 0x44, 0x8a); /* physical + broadcast; contiguous end-of-ring overflow */
    return 1;
}

void net_init(void)
{
    for (unsigned dev = 0; dev < 32; dev++) {
        uint32_t addr = 0x80000000u | (dev << 11);
        wr32(0xcf8, addr);
        if (rd32(0xcfc) != 0x813910ecu)
            continue;
        wr32(0xcf8, addr + 0x10);
        uint32_t bar = rd32(0xcfc);
        if (!(bar & 1) || !(bar & ~3u) || bar > 65535)
            continue;
        base = bar & ~3u;
        wr32(0xcf8, addr + 4);
        outw(0xcfc, rd16(0xcfc) | 5);
        outb(base + 0x52, 0);
        if (!reset_nic()) {
            base = 0;
            return;
        }
        for (unsigned i = 0; i < 6; i++)
            mac[i] = inb(base + i);
        state = NET_PROBING;
        return;
    }
}

static int send_frame(const uint8_t *frame, unsigned n)
{
    if (n > 1514 || !(rd32(base + 0x10 + slot * 4) & (1u << 13)))
        return 0;
    copy(tx[slot], frame, n);
    while (n < 60)
        tx[slot][n++] = 0;
    wr32(base + 0x20 + slot * 4, (uintptr_t)tx[slot]);
    wr32(base + 0x10 + slot * 4, n);
    slot = (slot + 1) % 4;
    return 1;
}

static void arp(unsigned op, const uint8_t *peer, const uint8_t *ip)
{
    uint8_t f[42] = {0};
    for (unsigned i = 0; i < 6; i++)
        f[i] = peer ? peer[i] : 255;
    copy(f + 6, mac, 6);
    put16(f + 12, 0x0806);
    put16(f + 14, 1);
    put16(f + 16, 0x0800);
    f[18] = 6;
    f[19] = 4;
    put16(f + 20, op);
    copy(f + 22, mac, 6);
    copy(f + 28, own_ip, 4);
    if (peer)
        copy(f + 32, peer, 6);
    copy(f + 38, ip, 4);
    send_frame(f, sizeof(f));
}

static uint16_t checksum(const uint8_t *p, unsigned n)
{
    uint32_t sum = 0;
    while (n >= 2) {
        sum += get16(p);
        p += 2;
        n -= 2;
    }
    if (n)
        sum += (unsigned)*p << 8;
    while (sum >> 16)
        sum = (sum & 65535) + (sum >> 16);
    return (uint16_t)~sum;
}

/* One outstanding A lookup, sent to QEMU's DNS forwarder on its own MAC. */
static const uint8_t dns_ip[4] = {10, 0, 2, 3};
static uint8_t query[272];
static unsigned query_len;
static uint16_t query_id;
static uint64_t query_deadline, dns_retry;
static char answer[24];
static const char *result = "Sin consulta";
static const char *dns_error;

/* Optional COM2 channel supplied by the Windows launcher. Each line carries
 * the DNS transaction ID so a delayed host lookup cannot complete a newer one. */
#define DNS_UART 0x2f8
static char host_request[272], host_line[32];
static unsigned host_len, host_sent, line_len;
static int host_available, host_checked, host_failed, discard_line;
static int parse_ipv4(const char *text, uint8_t *ip);

static void host_lookup_start(const char *name)
{
    dns_error = 0;
    if (!host_checked) {
        host_checked = 1;
        if (inb(DNS_UART + 5) != 0xff) {
            outb(DNS_UART + 1, 0);
            outb(DNS_UART + 3, 0x80);
            outb(DNS_UART, 1);
            outb(DNS_UART + 1, 0);
            outb(DNS_UART + 3, 3);
            outb(DNS_UART + 2, 0xc7);
            outb(DNS_UART + 4, 3);
            host_available = 1;
        }
    }
    if (!host_available) return;
    static const char hex[] = "0123456789abcdef";
    host_request[0] = '\n';
    host_request[1] = 'D';
    for (unsigned i = 0; i < 4; ++i)
        host_request[2+i] = hex[(query_id >> (12 - 4*i)) & 15];
    host_request[6] = ' ';
    host_len = 7;
    while (*name && host_len < sizeof(host_request)-1)
        host_request[host_len++] = *name++;
    host_request[host_len++] = '\n';
    host_sent = 0;
    host_failed = 0;
}

static void host_lookup_poll(void)
{
    if (!host_available || !query_len) return;
    if (host_sent < host_len && (inb(DNS_UART + 5) & 0x20)) {
        for (unsigned i=0; i<16 && host_sent<host_len; ++i)
            outb(DNS_UART, host_request[host_sent++]);
    }
    for (unsigned i=0; i<64 && (inb(DNS_UART + 5) & 1); ++i) {
        char ch = (char)inb(DNS_UART);
        if (ch != '\n') {
            if (line_len < sizeof(host_line)-1) host_line[line_len++] = ch;
            else discard_line = 1;
            continue;
        }
        host_line[line_len] = 0;
        uint8_t ip[4];
        if (!discard_line && line_len >= 7 && equal((uint8_t *)host_line,(uint8_t *)host_request+1,6)) {
            if (line_len == 7 && host_line[6] == '!') host_failed = 1;
            else if (line_len - 6 <= 15 && parse_ipv4(host_line+6,ip)) {
                copy((uint8_t *)answer,(uint8_t *)host_line+6,line_len-5);
                result = answer;
                query_len = 0;
            }
        }
        line_len = 0;
        discard_line = 0;
        if (!query_len) break;
    }
}

const char *net_dns_result(void) { return result; }

int net_lookup(const char *name, uint64_t now)
{
    if (!base || state != NET_READY) {
        result = "Sin enlace";
        return 0;
    }
    if (query_len)
        return 0;
    const char *domain = name;
    uint8_t encoded[255];
    unsigned pos = 0;
    while (*name) {
        if (pos >= sizeof(encoded) - 1)
            goto invalid;
        unsigned start = pos++;
        while (*name && *name != '.') {
            unsigned char ch = *name++;
            if (pos >= sizeof(encoded) - 1 || pos - start > 63 ||
                !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                  (ch >= '0' && ch <= '9') || ch == '-'))
                goto invalid;
            encoded[pos++] = ch;
        }
        if (pos == start + 1)
            goto invalid;
        encoded[start] = pos - start - 1;
        if (*name)
            name++;
    }
    if (!pos)
        goto invalid;
    encoded[pos++] = 0;
    put16(query, ++query_id);
    put16(query + 2, 0x0100);
    put16(query + 4, 1);
    copy(query + 12, encoded, pos);
    put16(query + 12 + pos, 1);
    put16(query + 14 + pos, 1);
    query_len = 16 + pos;
    result = "Consultando...";
    query_deadline = now + 8000;
    dns_retry = now;
    host_lookup_start(domain);
    return 1;
invalid:
    result = "Nombre de dominio no valido";
    return 0;
}

static void send_dns(const uint8_t *peer)
{
    uint8_t f[320] = {0};
    copy(f, peer, 6);
    copy(f + 6, mac, 6);
    put16(f + 12, 0x800);
    f[14] = 0x45;
    put16(f + 16, 28 + query_len);
    f[22] = 64;
    f[23] = 17;
    copy(f + 26, own_ip, 4);
    copy(f + 30, dns_ip, 4);
    put16(f + 24, checksum(f + 14, 20));
    put16(f + 34, 49152);
    put16(f + 36, 53);
    put16(f + 38, 8 + query_len);
    copy(f + 42, query, query_len);
    send_frame(f, 42 + query_len);
}

static unsigned skip_name(const uint8_t *p, unsigned pos, unsigned end)
{
    while (pos < end) {
        unsigned n = p[pos++];
        if (!n)
            return pos;
        if ((n & 0xc0) == 0xc0)
            return pos < end ? pos + 1 : 0;
        if (n > 63 || n > end - pos)
            return 0;
        pos += n;
    }
    return 0;
}

static void dns_reply(const uint8_t *p, unsigned end)
{
    if (end < query_len || get16(p) != query_id || !(p[2] & 128) || get16(p + 4) != 1 ||
        !equal(p + 12, query + 12, query_len - 12))
        return;
    const char *msg = "Sin registro IPv4";
    unsigned pos = query_len;
    if (p[2] & 2)
        msg = "Respuesta truncada";
    else if (p[3] & 15)
        msg = "Error del servidor DNS";
    else
        for (unsigned i = 0; i < get16(p + 6); i++) {
            pos = skip_name(p, pos, end);
            if (!pos || pos + 10 > end)
                return;
            unsigned n = get16(p + pos + 8);
            if (n > end - pos - 10)
                return;
            if (get16(p + pos) == 1 && get16(p + pos + 2) == 1 && n == 4) {
                unsigned dst = 0;
                for (unsigned j = 0; j < 4; j++) {
                    unsigned v = p[pos + 10 + j];
                    if (j)
                        answer[dst++] = '.';
                    if (v >= 100)
                        answer[dst++] = '0' + v / 100;
                    if (v >= 10)
                        answer[dst++] = '0' + v / 10 % 10;
                    answer[dst++] = '0' + v % 10;
                }
                answer[dst] = 0;
                msg = answer;
                break;
            }
            pos += 10 + n;
        }
    if (msg != answer && host_available) {
        dns_error = msg;
        return;
    }
    result = msg;
    query_len = 0;
}

enum ping_phase { PING_IDLE, PING_DNS, PING_ARP, PING_ECHO };
static struct {
    enum ping_phase phase;
    int ticket;
    uint8_t ip[4], next_hop[4], payload[16];
    uint64_t deadline, sent, retry;
    unsigned rtt;
    const char *message;
} ping;

static int parse_ipv4(const char *text, uint8_t *ip)
{
    for (int i = 0; i < 4; i++) {
        unsigned value = 0, digits = 0;
        while (*text >= '0' && *text <= '9') {
            value = value * 10 + (*text++ - '0');
            if (++digits > 3 || value > 255) return 0;
        }
        if (!digits || (i < 3 && *text++ != '.')) return 0;
        ip[i] = value;
    }
    return !*text;
}

static void ping_finish(const char *message)
{
    ping.phase = PING_IDLE;
    ping.message = message;
}

static void ping_route(uint64_t now)
{
    if (!ping.ip[0] || ping.ip[0] >= 224 ||
        (equal(ping.ip, own_ip, 3) && (ping.ip[3] == 255 || !ping.ip[3]))) {
        ping_finish("El destino debe ser una direccion unicast");
        return;
    }
    copy(ping.next_hop, equal(ping.ip, own_ip, 3) ? ping.ip : router, 4);
    ping.phase = PING_ARP;
    ping.deadline = now + 3000;
    ping.retry = now;
}

int net_ping(const char *target, uint64_t now, const char **error)
{
    if (ping.phase != PING_IDLE) { *error = "Hay un ping en curso"; return 0; }
    if (!base || state != NET_READY) { *error = "Sin enlace con la puerta de enlace"; return 0; }
    if (parse_ipv4(target, ping.ip)) {
        ping_route(now);
        if (ping.phase == PING_IDLE) { *error = ping.message; return 0; }
    } else {
        const char *p = target;
        while ((*p >= '0' && *p <= '9') || *p == '.') p++;
        if (!*p) { *error = "Direccion IPv4 no valida"; return 0; }
        if (query_len) { *error = "Hay una consulta DNS en curso"; return 0; }
        if (!net_lookup(target, now)) { *error = result; return 0; }
        ping.phase = PING_DNS;
    }
    ping.ticket = ping.ticket >= 0x7ffffffe ? 1 : ping.ticket + 1;
    ping.rtt = 0;
    ping.message = 0;
    return ping.ticket;
}

const char *net_ping_result(int ticket, unsigned *rtt_ms)
{
    *rtt_ms = 0;
    if (ticket != ping.ticket) return "La prueba ha finalizado";
    if (ping.phase != PING_IDLE) return 0;
    *rtt_ms = ping.rtt;
    return ping.message;
}

static void ping_send(const uint8_t *peer, uint64_t now)
{
    uint8_t f[58] = {0};
    copy(f, peer, 6); copy(f + 6, mac, 6);
    put16(f + 12, 0x800);
    f[14] = 0x45; put16(f + 16, sizeof(f) - 14);
    f[22] = 64; f[23] = 1;
    copy(f + 26, own_ip, 4); copy(f + 30, ping.ip, 4);
    put16(f + 24, checksum(f + 14, 20));
    f[34] = 8;
    put16(f + 38, 0x4d45); put16(f + 40, ping.ticket);
    copy(ping.payload, (const uint8_t *)"MettaOS!", 8);
    for (unsigned i = 0; i < 8; i++) ping.payload[8+i] = now >> (8*i);
    copy(f + 42, ping.payload, sizeof(ping.payload));
    put16(f + 36, checksum(f + 34, sizeof(f) - 34));
    if (send_frame(f, sizeof(f))) {
        ping.phase = PING_ECHO;
        ping.sent = now;
        ping.deadline = now + 3000;
    }
}

static void receive(uint8_t *f, unsigned n, uint64_t now)
{
    if (n >= 42 && get16(f + 12) == 0x0806 && get16(f + 14) == 1 && get16(f + 16) == 0x800 &&
        f[18] == 6 && f[19] == 4 && equal(f + 38, own_ip, 4)) {
        if (ping.phase == PING_ARP && get16(f + 20) == 2 &&
            equal(f + 28, ping.next_hop, 4) && equal(f + 32, mac, 6) && equal(f + 6, f + 22, 6))
            ping_send(f + 22, now);
        if (query_len && get16(f + 20) == 2 && equal(f + 28, dns_ip, 4) && equal(f + 32, mac, 6) &&
            equal(f + 6, f + 22, 6))
            send_dns(f + 22);
        if (get16(f + 20) == 1)
            arp(2, f + 22, f + 28);
        if (get16(f + 20) == 2 && equal(f + 28, router, 4) && equal(f + 32, mac, 6) &&
            equal(f + 6, f + 22, 6)) {
            if (state != NET_READY)
                KLOG_INFO("[net] RTL8139 gateway ARP confirmed\n");
            state = NET_READY;
            last_reply = now;
        }
        return;
    }
    if (query_len && n >= 42 && get16(f + 12) == 0x800 && f[14] == 0x45 && f[23] == 17 &&
        !(get16(f + 20) & 0x3fff) && equal(f, mac, 6) && equal(f + 26, dns_ip, 4) &&
        equal(f + 30, own_ip, 4) && get16(f + 34) == 53 && get16(f + 36) == 49152 &&
        !checksum(f + 14, 20)) {
        unsigned total = get16(f + 16), udp = get16(f + 38);
        if (total >= 28 && total <= n - 14 && udp >= 8 && udp == total - 20)
            dns_reply(f + 42, udp - 8);
        return;
    }
    /* Reply to IPv4 echo requests, validating lengths before touching payload. */
    if (n < 42 || get16(f + 12) != 0x0800 || f[14] != 0x45 || f[23] != 1 ||
        (get16(f + 20) & 0x3fff) || !equal(f + 30, own_ip, 4) || !equal(f, mac, 6))
        return;
    unsigned len = get16(f + 16);
    if (len < 28 || len > n - 14 || f[35] || checksum(f + 14, 20) ||
        checksum(f + 34, len - 20))
        return;
    if (!f[34] && ping.phase == PING_ECHO && equal(f + 26, ping.ip, 4) &&
        len == 28 + sizeof(ping.payload) && get16(f + 38) == 0x4d45 &&
        get16(f + 40) == (uint16_t)ping.ticket && equal(f + 42, ping.payload, sizeof(ping.payload))) {
        ping.rtt = (unsigned)(now - ping.sent);
        ping_finish("Respuesta");
    }
    if (f[34] != 8) return;
    copy(f, f + 6, 6);
    copy(f + 6, mac, 6);
    copy(f + 30, f + 26, 4);
    copy(f + 26, own_ip, 4);
    f[22] = 64;
    put16(f + 24, 0);
    put16(f + 24, checksum(f + 14, 20));
    f[34] = 0;
    put16(f + 36, 0);
    put16(f + 36, checksum(f + 34, len - 20));
    send_frame(f, len + 14);
}

int net_poll(uint64_t now)
{
    enum net_state old = state;
    if (!base) {
        if (ping.phase != PING_IDLE) ping_finish("Adaptador desconectado");
        return 0;
    }
    for (unsigned i = 0; i < 8 && !(inb(base + 0x37) & 1); i++) {
        __asm__ volatile("" ::: "memory");
        uint8_t *pkt = rx + cursor;
        unsigned status = pkt[0] | (pkt[1] << 8), len = pkt[2] | (pkt[3] << 8);
        if (len == 0xfff0)
            break;
        if (!(status & 1) || len < 4 || len > 1518) {
            state = NET_PROBING;
            if (!reset_nic()) {
                base = 0;
                state = NET_ABSENT;
            }
            break;
        }
        receive(pkt + 4, len - 4, now);
        cursor = ((cursor + len + 7) & ~3u) % RX_SIZE;
        outw(base + 0x38, (uint16_t)(cursor - 16));
        outw(base + 0x3e, 1);
    }
    host_lookup_poll();
    if (query_len && (now >= query_deadline || (dns_error && host_failed))) {
        query_len = 0;
        if (dns_error) result = dns_error;
        else result = host_failed ? "Windows no pudo resolver el dominio" : "DNS sin respuesta (8 s)";
    }
    if (base && query_len && now >= dns_retry) {
        arp(1, 0, dns_ip);
        dns_retry = now + 2000;
    }
    if (ping.phase == PING_DNS && !query_len) {
        if (result == answer && parse_ipv4(answer, ping.ip)) ping_route(now);
        else ping_finish(result);
    }
    if (ping.phase == PING_ARP || ping.phase == PING_ECHO) {
        if (now >= ping.deadline)
            ping_finish(ping.phase == PING_ARP ? "Sin respuesta ARP" : "Sin respuesta ICMP (3 s)");
        else if (ping.phase == PING_ARP && now >= ping.retry) {
            arp(1, 0, ping.next_hop);
            ping.retry = now + 1000;
        }
    }
    if (base && now >= next_probe) {
        if (now - last_reply > 15000)
            state = NET_PROBING;
        arp(1, 0, router);
        next_probe = now + 5000;
    }
    return old != state;
}
enum net_state net_status(void) { return state; }
const char *net_description(void)
{
    if (state == NET_READY)
        return "Puerta de enlace disponible";
    if (state == NET_PROBING)
        return "Buscando la puerta de enlace";
    return "Sin adaptador compatible";
}
