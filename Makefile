CC     = gcc
CFLAGS = -Wall -Wextra -std=c17 -pthread -g
P      ?= paso1

# Regla por defecto al ejecutar solo 'make'
all: servidor

servidor: src/servidor.c | bin
	$(CC) $(CFLAGS) $< -o bin/$@

paso%: src/paso%.c | bin
	$(CC) $(CFLAGS) $< -o bin/$@

tsan: src/$(P).c | bin
	$(CC) $(CFLAGS) -fsanitize=thread $< -o bin/$(P)-tsan

asan: src/$(P).c | bin
	$(CC) $(CFLAGS) -fsanitize=address,undefined $< -o bin/$(P)-asan

bin:
	mkdir -p bin

clean:
	rm -rf bin

.PHONY: all servidor tsan asan clean
