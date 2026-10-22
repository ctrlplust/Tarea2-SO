CC     = gcc
CFLAGS = -Wall -Wextra -std=c17 -pthread -g
# paso a compilar con sanitizers: make tsan P=paso2
P     ?= paso1

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

.PHONY: tsan asan clean
