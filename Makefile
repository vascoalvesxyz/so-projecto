CC := gcc -std=c99 -D_POSIX_C_SOURCE=200809L
FLAGS := -O2 -Wall -Wextra -Werror
DEBUG := -g 
LINKS := -z noexecstack -lpthread -lrt
SRC := controller_statistics.c controller_validator.c controller_miner_controller.c controller.c

.PHONY: install

install:
	${CC} ${FLAGS} ${SRC} -o controller ${LINKS}
	${CC} ${FLAGS} transaction-generator.c -o transaction-generator ${LINKS}

debug:
	${CC} ${DEBUG} ${FLAGS} ${SRC} -o controller ${LINKS}
	${CC} ${DEBUG} ${FLAGS} transaction-generator.c -o transaction-generator ${LINKS}
