#include <stdio.h>
#include <stdlib.h>

#include "transaction.h"

Transaction transcation_generate() {
    /* id_self, id_sender, id_reciever, timestamp, reward, value */
    return (Transaction) { 0, 1, 2, 3, 4, 5 };
}

int main(int argc, char *argv[]) {

    if(argc!=3) {
        printf("Wrong number of parameters\n");
        exit(0);
    }

    int reward = atoi(argv[1]);
    int time_ms = atoi(argv[2]);

    if(reward >=1 && reward <=3 && time_ms <=3000 && time_ms >=200){
        printf("The parameters are correct.\n");
    }


    return 0;
}
