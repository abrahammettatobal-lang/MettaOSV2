/*
 * MettaOS — cola circular de eventos de entrada (kernel). SA-13
 */
#ifndef METTA_INPUT_EVENT_QUEUE_H
#define METTA_INPUT_EVENT_QUEUE_H

#include <stddef.h>
#include <input/input.h>

void event_queue_reset(void);
int  event_queue_push(const input_event_t *ev);
int  event_queue_pop(input_event_t *out);
size_t event_queue_pending(void);

#endif /* METTA_INPUT_EVENT_QUEUE_H */