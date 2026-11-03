CC     = gcc
CFLAGS = -Wall -Wextra -std=c17 -pthread -g
P      ?= paso1
BASE    = src/tablero.c

# Regla por defecto al ejecutar solo 'make'
all: torneo

# el enunciado pide ./torneo, asi que el binario final va en la raiz
torneo: src/torneo.c src/mesa.c src/config.c $(BASE)
	$(CC) $(CFLAGS) $^ -o $@

# paso1 todavia tiene su propio tablero adentro: se compila solo
paso1: src/paso1.c | bin
	$(CC) $(CFLAGS) $< -o bin/$@

paso%: src/paso%.c $(BASE) | bin
	$(CC) $(CFLAGS) $^ -o bin/$@

paso4: src/paso4.c src/mesa.c $(BASE) | bin
	$(CC) $(CFLAGS) $^ -o bin/$@

paso8: src/paso8.c src/mesa.c $(BASE) | bin
	$(CC) $(CFLAGS) $^ -o bin/$@

tsan: src/$(P).c src/mesa.c src/config.c $(BASE) | bin
	$(CC) $(CFLAGS) -fsanitize=thread $^ -o bin/$(P)-tsan

asan: src/$(P).c src/mesa.c src/config.c $(BASE) | bin
	$(CC) $(CFLAGS) -fsanitize=address,undefined $^ -o bin/$(P)-asan

bin:
	mkdir -p bin

clean:
	rm -rf bin torneo

.PHONY: all torneo tsan asan clean
