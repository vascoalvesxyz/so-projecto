CC := gcc -std=c99 -D_POSIX_C_SOURCE=200809L
FLAGS := -O2 -Wall -Wextra -Werror
DEBUG := -g -DDEBUG 
LINKS := -z noexecstack -lpthread -lrt -lcrypto
SRC := d_pow.c d_statistics.c d_validator.c d_miner_controller.c deichain.c

.PHONY: install

install:
	${CC} ${FLAGS} ${SRC} -o deichain ${LINKS}
	${CC} ${FLAGS} transaction-generator.c -o TxGen  ${LINKS}

debug:
	${CC} ${DEBUG} ${FLAGS} ${SRC} -o deichain ${LINKS}
	${CC} ${DEBUG} ${FLAGS} transaction-generator.c -o TxGen ${LINKS}
