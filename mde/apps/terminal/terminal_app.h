#ifndef MDE_TERMINAL_APP_H
#define MDE_TERMINAL_APP_H

#include "terminal.h"

int  mde_terminal_app_start(void);
void mde_terminal_app_key(char c);
mde_terminal_t *mde_terminal_app_view(void);

#endif /* MDE_TERMINAL_APP_H */