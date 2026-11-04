CC     = gcc
CFLAGS = -Wall -Wextra -std=c17 -pthread -g
P      ?= paso1
BASE    = src/tablero.c

all: torneo

# Binario de torneo (según pide el enunciado en la raíz)
torneo: src/torneo.c src/mesa.c src/config.c $(BASE)
	$(CC) $(CFLAGS) $^ -o $@

# Binario del servidor unificado
servidor: src/servidor.c src/mesa.c src/config.c $(BASE) | bin
	$(CC) $(CFLAGS) $^ -o bin/$@

# Reglas para pasos individuales
paso1: src/paso1.c | bin
	$(CC) $(CFLAGS) $< -o bin/$@

paso%: src/paso%.c $(BASE) | bin
	$(CC) $(CFLAGS) $^ -o bin/$@

paso4: src/paso4.c src/mesa.c $(BASE) | bin
	$(CC) $(CFLAGS) $^ -o bin/$@

paso8: src/paso8.c src/mesa.c $(BASE) | bin
	$(CC) $(CFLAGS) $^ -o bin/$@

# Sanitizers para depuración de hilos y memoria
tsan: src/$(P).c src/mesa.c src/config.c $(BASE) | bin
	$(CC) $(CFLAGS) -fsanitize=thread $^ -o bin/$(P)-tsan

asan: src/$(P).c src/mesa.c src/config.c $(BASE) | bin
	$(CC) $(CFLAGS) -fsanitize=address,undefined $^ -o bin/$(P)-asan

bin:
	mkdir -p bin

clean:
	rm -rf bin torneo

.PHONY: all torneo servidor tsan asan clean
