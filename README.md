# Komikan Concurrente — Tarea 2 SO

Torneo de Komikan en tiempo real con hilos POSIX: matchmaker, M mesas concurrentes, juez por mesa y 13 fichas por partida.

**Integrantes:**
Tomas Nunez - 
Benjamin Zuniga

## Compilar

```sh
make                  # ./torneo (binario final)
make pasoN            # src/pasoN.c -> bin/pasoN
make tsan P=torneo    # con ThreadSanitizer  -> bin/torneo-tsan
make asan P=torneo    # con AddressSanitizer -> bin/torneo-asan
make clean
```

## Uso

```sh
./torneo N M [config.txt]
```

- `N` jugadores: se emparejan de a 2, el primero de cada pareja hace de Puma.
- `M` mesas simultaneas como maximo (semaforo: el resto espera pasivamente).
- Si `N` es impar, el ultimo jugador se queda sin rival y se informa.
- Cada partida corta a los 30 s o 5000 movimientos para evitar livelock.
- `Ctrl+C` cierra el servidor: corta las partidas, libera las mesas y termina sin fugas.

### config.txt

```
CONFIG SEED 42 DELAY_MIN 50 DELAY_MAX 300 PUMA_INIT 2 2
DESCONEXION_PCT 0      # opcional: % de movimientos en que una ficha "se cae"
TIMEOUT_MS 2000        # opcional: ms sin latido para declarar abandono
```

Se usa la **primera** linea `CONFIG` del archivo. `tests/test_cases.txt` trae 12
casos: se corre uno por vez recortando el archivo o pasando un config propio.
Los `EVENT` de ese archivo todavia no se interpretan; los movimientos los eligen
las fichas al azar con la semilla configurada.

## Estructura

- `src/tablero.c` — tablero como grafo, saltos y reglas (paso 1)
- `src/paso2.c` — 13 hilos sin sincronizacion: la carrera a proposito
- `src/paso3.c` — el mismo juego con un mutex por mesa
- `src/paso3b.c` — el paso 3 con semaforo binario (comparacion para el informe)
- `src/mesa.c` — juez, cond var, victoria y abandonos (paso 4)
- `src/paso8.c` — una ficha se cae con el lock tomado; mutex robusto vs comun
- `src/torneo.c` — matchmaker, semaforo de M mesas y SIGINT
- `src/config.c` — parser de `config.txt`
- `tests/` casos de prueba (`test_cases.txt`)
- `docs/` enunciado
