CC := gcc
FLAGS := -O2
DEBUG_FLAGS := -g
# LINKS := 

.PHONY: install

install:
	${CC} ${FLAGS} controller.c -o controller

debug:
	${CC} ${DEBUG_FLAGS} controller.c -o controller
