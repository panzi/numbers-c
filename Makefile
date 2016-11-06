CC=gcc
#CC=clang
CFLAGS=-Wall -Wextra -Werror -std=gnu11 -O2 -pthread
#CFLAGS=-Wall -Wextra -Werror -std=gnu11 -O2 -pthread -g
LIB=-lm
OBJ=build/main.o build/numbers.o

.PHONY: all clean

all: build/numbers

build/numbers: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LIB) -o $@

build/%.o: src/%.c
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm -f $(OBJ) build/numbers
