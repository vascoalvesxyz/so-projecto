#include "controller.h"

void c_stat_main() {
    //Gerar Estatisticas
    // E escrever antes de acabar a simulação (Isto é a simulação acaba e enquanto fecha)
    void handle_signit() {
        puts("STATISTICS: EXITED SUCCESSFULLY!\n");
        c_cleanup();
        exit(EXIT_SUCCESS);
    }

    signal(SIGINT, handle_signit);

    while (1) { }
}
