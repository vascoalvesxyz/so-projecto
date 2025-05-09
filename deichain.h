#ifndef __DEICHAIN_H__
#define __DEICHAIN_H__

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TX_ID_LEN 64
#define TXB_ID_LEN 64
#define HASH_SIZE 65  // SHA256_DIGEST_LENGTH * 2 + 1

extern size_t transactions_per_block;

// Transaction structure
// Inline function to compute the size of a BlockInfo
static inline size_t get_transaction_block_size() {

  if (transactions_per_block == 0) {
    perror("Must set the 'transactions_per_block' variable before using!\n");
    exit(-1);
  }

  return sizeof(BlockInfo) +
         transactions_per_block * sizeof(Transaction);
}

#endif
