#include "controller.h"

int running = 1;

void val_exit(int retv) {

    if (pipe_validator_fd < 0) {
        close(pipe_validator_fd);
    }

    c_cleanup();

    if (retv == 0) {
        puts("Validator: Exited successfully!\n");
        exit(EXIT_SUCCESS);
    }
    puts("Validator: FAILED!\n");
    exit(EXIT_FAILURE);
}

void val_handle_signit() {
    val_exit(0);
}

void c_val_main() {

    /* Open pipe (not blocking to check errors) */
    pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY | O_NONBLOCK);
    if (pipe_validator_fd < 0) {
        puts("Validator: Failed to open FIFO\n");
        val_exit(1);
    }

    close(pipe_validator_fd);
    signal(SIGINT, val_handle_signit);

    pipe_validator_fd = open(PIPE_VALIDATOR, O_RDONLY);

    Transaction transaction_recieved;

    while (1) {
        ssize_t count = read(pipe_validator_fd, (char* )&transaction_recieved, sizeof(Transaction));

        if (count < 0) {
            puts("READ ERROR");
            val_exit(1);
        } else if (count == 0) {
            continue;
        }
        printf("[Validator] RECIEVED TRANSACTION ID=%d\n", transaction_recieved.id_self);
    }

    val_exit(0);
}
