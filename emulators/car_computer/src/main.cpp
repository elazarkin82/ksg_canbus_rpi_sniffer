#include "CarComputer.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

volatile sig_atomic_t g_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <can_interface>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_sigint);

    printf("Starting Car Computer Emulator on %s...\n", argv[1]);

    CarComputer computer(argv[1]);
    if (!computer.start()) {
        return 1;
    }

    while (g_running) {
        sleep(1);
    }

    printf("\nStopping...\n");
    computer.stop();

    return 0;
}
