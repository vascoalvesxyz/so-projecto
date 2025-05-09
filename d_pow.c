//Vasco Alves 2022228207 Joao Neto 2023234004
#include "deichain.h"

void c_pow_block_serialize(TransactionBlock input, byte_t *serial) {

  memset(serial, 0, SIZE_BLOCK);
  memcpy(serial, (void*) input, SIZE_BLOCK);

  BlockInfo *bf = (BlockInfo*) serial;
  bf->nonce = 0;
}

void c_pow_hash_compute(TransactionBlock input, hash_t *output) {

  /* Serialize transaction data */
  byte_t data_serial[SIZE_BLOCK];
  c_pow_block_serialize(input, data_serial);

  /* Clean buffer and write hash */
  memset(output, 0, 32);
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
  Transaction *tarray_end = tarray + g_transactions_per_block;
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
  int byte = 0;
  DifficultyLevel difficulty = _get_dificulty(reward);

  /* check the hash directly */
  char hex[2] = {0x00, 0x00};
  for (byte = 0; byte < HASH_SIZE ; byte++) {
    hex[0] = hash[byte] & 0xF0;
    hex[1] = hash[byte] & 0x0F;

    /* Check first hex value */
    if (hex[0] != 0x0) {
      break;
    }

    /* Both are 0 */
    if (hex[1] == 0x0) {
      zeros += 2;
      continue;
    } 
    /* Second hex value is not zero */
    else {
      zeros += 1;
      break;
    }

  }

  /* At least minimum zeros must exist */
  if (zeros < minimum)
    return 0;

  switch (difficulty) {
    case EASY:  // 0000[0-b]
      /* for even zeros, the next hex is hex[0] */
      if ((zeros == 4 && hex[0] <= 0xB) || zeros > 4) return 1;
      break;
    case NORMAL:  // 00000
      if (zeros >= 5) return 1;
      break;
    case HARD:  // // 00000[0-b]
      /* for uneven zeros, the next hex is hex[1] */
      if ((zeros == 5 && hex[1] <= 0xB) || zeros > 5) return 1;
      break;
    default:
      fprintf(stderr, "Invalid Difficult\n");
      exit(2);
  }

  return 0;
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

    /* Check if hash is correct */
    if (c_pow_checkdifficulty(hash, reward)) {
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
    fprintf(stderr, "[POW] Giving up\n");
    result.error = 1;
  }

  result.operations = block->nonce + 1;
  return result;
}
