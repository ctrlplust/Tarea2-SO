# Komikan Concurrente — Tarea 2 SO

Torneo de Komikan en tiempo real con hilos POSIX: matchmaker, M mesas concurrentes, juez por mesa y 13 fichas por partida.

**Integrantes:** TODO: nombre — rol

**Estado:** en desarrollo.

## Compilar

```sh
make pasoN            # src/pasoN.c -> bin/pasoN
make tsan P=pasoN     # con ThreadSanitizer  -> bin/pasoN-tsan
make asan P=pasoN     # con AddressSanitizer -> bin/pasoN-asan
make clean
```

## Uso (versión final)

```sh
./torneo N M [config.txt]
```

## Estructura

- `src/` código (`paso1.c`, `paso2.c`, …)
- `tests/` casos de prueba (`test_cases.txt`)
- `docs/` enunciado e informe
