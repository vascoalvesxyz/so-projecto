CC := gcc -std=c99 -D_POSIX_C_SOURCE=200809L
FLAGS := -O2 -Wall -Wextra
DEBUG := -g 
LINKS := -z noexecstack -lpthread -lrt

.PHONY: install

install:
	${CC} ${FLAGS} controller_miner_controller.c controller.c -o controller ${LINKS}
	${CC} ${FLAGS} transaction-generator.c -o transaction-generator ${LINKS}

debug:
	${CC} ${DEBUG} ${LINKS} ${FLAGS} controller.c -o controller
	${CC} ${DEBUG} ${FLAGS} transaction-generator.c -o transaction-generator
