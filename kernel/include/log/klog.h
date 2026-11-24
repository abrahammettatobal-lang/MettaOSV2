#ifndef METTA_LOG_KLOG_H
#define METTA_LOG_KLOG_H

/*
 * Diagnóstico por serie (COM1) — complementa serial/printk (SA-06).
 * Prefijos alineados con bootsplash y arranque temprano.
 */

typedef enum klog_level {
    KLOG_LEVEL_ERR = 0,
    KLOG_LEVEL_WARN,
    KLOG_LEVEL_INFO,
    KLOG_LEVEL_DEBUG,
} klog_level_t;

#ifndef KLOG_DEFAULT_LEVEL
#define KLOG_DEFAULT_LEVEL KLOG_LEVEL_INFO
#endif

klog_level_t klog_get_level(void);
void klog_set_level(klog_level_t level);

void klog_emit(klog_level_t level, const char *fmt, ...);

#define KLOG_ERR(fmt, ...)   klog_emit(KLOG_LEVEL_ERR,   fmt, ##__VA_ARGS__)
#define KLOG_WARN(fmt, ...)  klog_emit(KLOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define KLOG_INFO(fmt, ...)  klog_emit(KLOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define KLOG_DEBUG(fmt, ...) klog_emit(KLOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

/* Mensajes de arranque / bootsplash en serie */
#define KLOG_BOOT(fmt, ...)  klog_emit(KLOG_LEVEL_INFO, "[boot] " fmt, ##__VA_ARGS__)

#endif /* METTA_LOG_KLOG_H */