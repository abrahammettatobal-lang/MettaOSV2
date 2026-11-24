# Cola de misiones (coordinador)

## Oleada 1 — activa (10 RUNNING)
SA-01, SA-02, SA-03, SA-04, SA-06, SA-07, SA-14, SA-18, SA-21, SA-35

## Oleada 2 — tras integración Fase 1 parcial
NOT_STARTED: SA-05 (cpu-structures-engineer)
COMPLETED: SA-12 (debugging-diagnostics-engineer — panic/KLOG/GDB notes)

## Oleada 3 — Fase 2 memoria
WAITING_FOR_DEPENDENCY: SA-08, SA-09, SA-10, SA-11 (requieren PMM + contratos SA-01)

## Restantes Fase 3–7
NOT_STARTED: SA-13, SA-15, SA-16, SA-17, SA-19, SA-20, SA-22, SA-23–34

## Cierre oleada 1 (coordinador)
COMPLETED: [SA-04](8dcbb587-beb3-4501-9568-8bc46a828917), [SA-06](e02c2381-daaa-4ebb-afec-635b1d3c34cf), [SA-07](91532f75-4e2f-4e0f-999d-5f4d9a03c173), [SA-14](dfc73a54-d97f-4536-b964-02b63c051fe8), [SA-18](a59fce3d-4e5d-4981-b460-755edcd5b05c), [SA-21](e4784cec-6aac-4f2d-9342-5873e0ea0bef). Handoff rev1 en `kernel/boot/handoff.c`.

## Integración pendiente (bloqueada)
- [SA-02](fd28b807-2736-4b6a-b01c-2870af959ee9), [SA-03](ca753694-35e3-4f9a-a3f3-a1ecd66cb126), [SA-05](f8144ff5-3fef-426b-bea2-df5c036437da): aún RUNNING → reanudar al cerrar con build/IDT.
- [SA-35](8ec72c6b-538c-42a8-b50a-36c0a92b20b3): reanudado para smoke tras toolchain.