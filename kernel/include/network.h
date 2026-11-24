#ifndef METTA_NETWORK_H
#define METTA_NETWORK_H
#include <stdint.h>

enum net_state { NET_ABSENT, NET_PROBING, NET_READY };
void net_init(void);
int net_poll(uint64_t now_ms);
enum net_state net_status(void);
const char *net_description(void);
int net_lookup(const char *name, uint64_t now);
const char *net_dns_result(void);
/* One asynchronous Echo Request. A positive ticket identifies its result. */
int net_ping(const char *target, uint64_t now, const char **error);
const char *net_ping_result(int ticket, unsigned *rtt_ms);
#endif
