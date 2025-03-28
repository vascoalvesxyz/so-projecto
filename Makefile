CC := gcc
FLAGS := -O2
DEBUG_FLAGS := -g
# LINKS := 

.PHONY: install

install:
	${CC} -Wall -Wextra ${FLAGS} controller.c -o controller

debug:
	${CC} ${DEBUG_FLAGS} controller.c -o controller
