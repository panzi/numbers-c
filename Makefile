CC=gcc
#CC=clang
CFLAGS=-Wall -Wextra -Werror -pedantic -std=c11 -O2 -pthread
OBJ=build/main.o build/numbers.o build/expr.o build/exprbuf.o

ifeq ($(DEBUG),ON)
	CFLAGS+=-g -DDEBUG
else
	CFLAGS+=-DNDEBUG
endif

.PHONY: all clean

all: build/numbers

build/numbers: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

build/%.o: src/%.c src/panic.h
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm -f $(OBJ) build/numbers
