#include "controller.h"

void c_pow_block_serialize(const TransactionBlock input, const unsigned char* serial) {

  memset(serial, 0, SHMEM_SIZE_BLOCK);
  memcpy(serial, input, SHMEM_SIZE_BLOCK);

  BlockInfo *bf = (BlockInfo*) serial;
  bf->nonce = 0;
}

void c_pow_hash_compute(const TransactionBlock input, hash_t output) {

  /* Serialize transaction data */
  unsigned char data_serial[SHMEM_SIZE_BLOCK];
  c_pow_block_serialize(input, data_serial);

  /* Clean buffer and write hash */
  memset(output, 0, 32);
  SHA256(data_serial, SHMEM_SIZE_BLOCK, output);
}

void c_pow_hash_to_string(hash_t input, hashstring_t output) {

  for (int i = 0; i < TX_ID_LEN; i++) {
    snprintf(output[i*2], 2, "%02x", input[i]);
  }

  output[64] = '\0'
}

int c_pow_getmaxreward(const TransactionBlock tb) {
  if (tb == NULL) return 0;

  const Transaction *tarray = (Transaction*) ((char*) tb + sizeof(BlockInfo));
  const Transaction *tarray_end = tarray + g_transaction_per_block;
  int max_reward = tarray->reward;
  int reward = 0;

  for (Transaction *t_ptr = tarray; t_ptr != tarray_end; t_ptr++) {
    reward = t_ptr->reward;
    if (reward > max_reward) {
      max_reward = reward;
    }
  }

  return max_reward;
}

static DifficultyLevel _get_dificulty(const int reward) {
  if (reward <= 1)
    // 0000
    return EASY;
  if (reward == 2)
    // 00000
    return NORMAL;
  // For reward > 2
  // 00000[0-b]
  return HARD;
}

int c_pow_checkdifficulty(const hash_t hash, const int reward) {
  int minimum = 4;
  int zeros = 0;
  DifficultyLevel difficulty = _get_dificulty(reward);

  // TODO check the hash directly
  // 0b1 &&
  // 0b11 && 
  // 0b111 && 
  
  hashstring_t hash_str[HASH_STR]
  while (hash[zeros] == '0') {
    zeros++;
  }

  // At least `minimum` zeros must exist
  if (zeros < minimum) return 0;

  char next_char = hash[zeros];

  switch (difficulty) {
    case EASY:  // 0000[0-b]
      if ((zeros == 4 && next_char <= 'b') || zeros > 4) return 1;
      break;
    case NORMAL:  // 00000
      if (zeros >= 5) return 1;
      break;
    case HARD:  // // 00000[0-b]
      if ((zeros == 5 && next_char <= 'b') || zeros > 5) return 1;
      break;
    default:
      fprintf(stderr, "Invalid Difficult\n");
      exit(2);
  }

  return 0;
}


int c_pow_verify_nonce(const TransactionBlock tb) {

  hash_t hashbuf[HASH_SIZE];
  c_pow_hash_compute(tb, hashbuf);

  BlockInfo block_info = *((BlockInfo*) tb);
  int reward = c_pow_getmaxreward(tb);
  return check_difficulty(hash, reward);
}

DifficultyLevel getDifficultFromReward(const int reward);

PoWResult c_pow_proofofwork(const TransactionBlock *input) 

void compute_sha256(const BlockInfo *input, char *output);

