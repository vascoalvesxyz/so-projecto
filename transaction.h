#ifndef _TRANSACTION_H_
#define _TRANSACTION_H_

#include <stdint.h>
typedef struct Transaction {
    uint32_t id_self;       // 4 bytes
    uint32_t id_sender;     // 4 bytes
    uint32_t id_reciever;   // 4 bytes
    uint32_t timestamp;     // 4 bytes (beware of 2032)
    uint8_t reward;         // 1 bytes
    uint8_t value;          // 1 bytes 
} Transaction;

#endif
