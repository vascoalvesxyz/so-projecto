//Vasco Alves 2022228207 Joao Neto 2023234004
#include "deichain.h"
#include <stdio.h>
  
void c_pow_block_serialize(TransactionBlock input, byte_t *serial) {
  memset(serial, 0, SIZE_BLOCK);
  memcpy(serial, (void*) input, SIZE_BLOCK);
}

void c_pow_hash_compute(TransactionBlock input, hash_t *output) {

  /* Serialize transaction data */
  byte_t data_serial[SIZE_BLOCK];
  c_pow_block_serialize(input, data_serial);

  /* Clean buffer and write hash */
  SHA256(data_serial, SIZE_BLOCK, output);
}

void c_pow_hash_to_string(hash_t *input, hashstring_t *output) {

  /* Since HEX values only represent half of a byte,
   * the resulting string is twice as long
   * the additional character terminates the string */
  for (int i = 0; i < HASH_SIZE; i++) {
    snprintf(&output[i*2], 3, "%02x", input[i]);
  }

  output[64] = '\0'; // just to be safe
}

int c_pow_getmaxreward(TransactionBlock tb) {
  if (tb == NULL) return 0;

  Transaction *tarray = (Transaction*) ((char*) tb + sizeof(BlockInfo));
  Transaction *tarray_end = tarray + config.transactions_per_block;
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

static DifficultyLevel _get_dificulty(int reward) {
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

int c_pow_checkdifficulty(hash_t *hash, int reward) {

  int minimum = 4;
  int zeros = 0;
  DifficultyLevel difficulty = _get_dificulty(reward);

  /* check the hash directly */
  hashstring_t hashstring[HASH_STRING_SIZE];
  c_pow_hash_to_string(hash, hashstring);

  int idx = 0;
  while (hashstring[idx++] == '0') {
    zeros += 1;
  }

  /* At least minimum zeros must exist */
  if (zeros < minimum) {
    return 0;
  }

  char next_byte = hashstring[zeros+1];

  switch (difficulty) {
    case EASY: // 4 hex zeros (16 bits)
      if ((zeros == 16 && next_byte <= 'b') || zeros > 16)
        return 1;
      break;
    case NORMAL: // 5 hex zeros (20 bits)
      if (zeros >= 20)
        return 1;
      break;
    case HARD: // 5 hex zeros (20 bits) + next byte <= 'b'
      if ((zeros == 20 && next_byte <= 'b') || zeros > 20)
        return 1;
      break;
    default:
      return 0;
  }

  return 1;
}

int c_pow_verify_nonce(TransactionBlock tb) {

  hash_t hashbuf[HASH_SIZE];
  c_pow_hash_compute(tb, hashbuf);

  int reward = c_pow_getmaxreward(tb);
  return c_pow_checkdifficulty(hashbuf, reward);
}

PoWResult c_pow_proofofwork(TransactionBlock *tb) {

  PoWResult result = {
    .hash = {0},
    .elapsed_time = 0.0,
    .operations = 0,
    .error = 0
  };

  BlockInfo *block = (BlockInfo*) tb;
  block->nonce = 0;

  hash_t hash[HASH_SIZE];
  int reward = c_pow_getmaxreward(block);

  /* Start tracking time */
  clock_t start = clock();
  while (block->nonce <= POW_MAX_OPS) {
    /* Compute hash */
    c_pow_hash_compute(tb, hash);

    #ifdef DEBUG
    // printf("[POW] attempt %ld\n", block->nonce);
    #endif /* ifdef DEBUG */

    /* Check if hash is correct */
    if (1 == c_pow_checkdifficulty(hash, reward)) {
      result.elapsed_time = (double)(clock() - start) / CLOCKS_PER_SEC;
      memcpy(result.hash, hash, HASH_SIZE);
      break;
    }

    /* Increment nonce and try again */
    block->nonce++;
  }
  
  /* Stop tracking time */
  result.elapsed_time = (double)(clock() - start) / CLOCKS_PER_SEC;

  /* Check if while ran until failure */
  if (block->nonce > POW_MAX_OPS) {
    result.error = 1;
  }

  result.operations = block->nonce + 1;

  return result;
}
int c_pow_hash_equals(hash_t* hash1, hash_t* hash2){
  hashstring_t temp[HASH_STRING_SIZE];
  hashstring_t temp1[HASH_STRING_SIZE];
  c_pow_hash_to_string(hash1, temp);
  c_pow_hash_to_string(hash2, temp1);
  return strcmp(temp, temp1);


}
