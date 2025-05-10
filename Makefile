CC := gcc -std=c99 -D_POSIX_C_SOURCE=200809L
FLAGS := -O2 -Wall -Wextra -Werror
DEBUG := -g -DDEBUG 
LINKS := -z noexecstack -lpthread -lrt -lcrypto
SRC := d_pow.c d_statistics.c d_validator.c d_miner_controller.c deichain.c
OBJS := $(SRC:.c=.o)
TXGEN_SRC := transaction-generator.c
TXGEN_OBJ := $(TXGEN_SRC:.c=.o)

.PHONY: all

all: deichain TxGen clean 

debug: FLAGS += $(DEBUG)
debug: all

deichain: $(OBJS)
	$(CC) $(FLAGS) $^ -o deichain $(LINKS)

TxGen: $(TXGEN_OBJ)
	$(CC) $(FLAGS) $^ -o TxGen $(LINKS)

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TXGEN_OBJ)
