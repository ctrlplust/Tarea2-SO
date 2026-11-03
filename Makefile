CC     = gcc
CFLAGS = -Wall -Wextra -std=c17 -pthread -g
P      ?= paso1
BASE    = src/tablero.c

# Regla por defecto al ejecutar solo 'make'
all: paso1

torneo: src/torneo.c src/mesa.c src/config.c $(BASE) | bin
	$(CC) $(CFLAGS) $^ -o bin/$@

# paso1 todavia tiene su propio tablero adentro: se compila solo
paso1: src/paso1.c | bin
	$(CC) $(CFLAGS) $< -o bin/$@

paso%: src/paso%.c $(BASE) | bin
	$(CC) $(CFLAGS) $^ -o bin/$@

tsan: src/$(P).c $(BASE) | bin
	$(CC) $(CFLAGS) -fsanitize=thread $^ -o bin/$(P)-tsan

asan: src/$(P).c $(BASE) | bin
	$(CC) $(CFLAGS) -fsanitize=address,undefined $^ -o bin/$(P)-asan

bin:
	mkdir -p bin

clean:
	rm -rf bin

.PHONY: all torneo tsan asan clean
