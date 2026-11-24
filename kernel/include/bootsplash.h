/* MettaOS kernel — contrato bootsplash post-MDE (SA-21)
 * Sin implementación de dibujo en kernel aún; solo API y temporización.
 */

#ifndef METTAOS_KERNEL_BOOTSPLASH_H
#define METTAOS_KERNEL_BOOTSPLASH_H

#include <stdint.h>

/** Tiempo de retención del splash tras señal «MDE listo» (ms). */
#define BOOTSPLASH_POST_MDE_HOLD_MS 500u

typedef enum bootsplash_phase {
    BOOTSPLASH_PHASE_EARLY = 0,  /* arranque kernel, pre-MDE */
    BOOTSPLASH_PHASE_MDE,        /* MDE inicializando */
    BOOTSPLASH_PHASE_HOLD,       /* MDE listo: retención 2000 ms */
    BOOTSPLASH_PHASE_RELEASED,   /* framebuffer cedido al shell */
} bootsplash_phase_t;

/** Primer pintado del splash tras fb_init (gradiente + marca). */
void bootsplash_early_init(void);

/** Avance por frame: retención post-MDE y repintado mientras no RELEASED. */
void bootsplash_tick(uint64_t monotonic_ms);

/**
 * Llamar cuando el subsistema MDE reporta escritorio listo.
 * @param monotonic_ms tiempo monotónico desde arranque (misma base que should_hold).
 */
void bootsplash_on_mde_ready(uint64_t monotonic_ms);

/**
 * @return fase actual del contrato (consulta sin efectos laterales).
 */
bootsplash_phase_t bootsplash_phase(void);

/**
 * @return distinto de 0 mientras el splash debe permanecer visible
 *         durante el intervalo post-MDE (BOOTSPLASH_POST_MDE_HOLD_MS).
 */
int bootsplash_should_hold(uint64_t monotonic_ms);

/**
 * Marca fin de retención y transición a shell (invocar tras hold o por timeout).
 */
void bootsplash_release(void);

#endif /* METTAOS_KERNEL_BOOTSPLASH_H */