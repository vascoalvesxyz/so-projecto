#include "controller.h"

void c_val_main() {
    void handle_signit() {
        c_logputs("Validator: Exited successfully!\n");
        c_cleanup();
        exit(EXIT_SUCCESS);
    }

    signal(SIGINT, handle_signit);
    while (1) {

    }
}
