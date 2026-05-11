#include "ripple.h"
#include <signal.h>
#include <stdio.h>

void signals_setup(void) {
    struct sigaction sa_term = { .sa_handler = SIG_DFL };
    struct sigaction sa_alrm = { .sa_handler = SIG_DFL };
    sigemptyset(&sa_term.sa_mask);
    sigemptyset(&sa_alrm.sa_mask);
    sigaction(SIGTERM, &sa_term, NULL);
    sigaction(SIGALRM, &sa_alrm, NULL);
    /* SIGPIPE ignored to prevent crash on broken WebSocket connections */
    signal(SIGPIPE, SIG_IGN);
}
