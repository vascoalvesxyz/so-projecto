CC := gcc 
FLAGS := -O2 -Wall -Wextra
DEBUG := -g 
LINKS := -z noexecstack  -lpthread -lrt

.PHONY: install

install:
	${CC} ${FLAGS} ${LINKS} controller_miner_controller.c controller.c -o controller
	${CC} ${FLAGS} transaction-generator.c -o transaction-generator

debug:
	${CC} ${DEBUG} ${LINKS} ${FLAGS} controller.c -o controller
	${CC} ${DEBUG} ${FLAGS} transaction-generator.c -o transaction-generator
