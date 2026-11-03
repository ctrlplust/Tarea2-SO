# torneo de komikan concurrente

el torneo lee un `config.txt` opcional, arma N jugadores, los empareja de a dos y
corre cada partida en una mesa con 13 hilos: un puma y 12 perros moviendose al
mismo tiempo, sin turnos. nunca hay mas de **M** mesas vivas al mismo tiempo.

las cosas que hay que tener a mano:

```sh
make                        # compila ./torneo
./torneo 20 2               # 20 jugadores, 2 mesas
./torneo 6 2 mi_config.txt  # con configuracion propia
make paso2 && ./bin/paso2   # la prueba de la carrera
```

## como esta armado el repo

```
README.md          este archivo
Makefile           compilar y limpiar
src/torneo.c       el entregable: matchmaker, semaforo de mesas y SIGINT
src/mesa.c         la partida: 13 hilos, juez, latidos y fin
src/tablero.c      el tablero como grafo, los saltos y las reglas
src/config.c       el parser de config.txt
src/paso1.c ...    la construccion por partes, un archivo por paso
tests/             test_cases.txt, los 12 casos de la pauta
docs/              el enunciado en pdf
```

`./torneo` queda en la raiz a proposito: el enunciado pide ejecutar
`./torneo N M [config.txt]` tal cual.

## como compila

```sh
gcc -Wall -Wextra -std=c17 -pthread -g -o torneo \
    src/torneo.c src/mesa.c src/config.c src/tablero.c
```

son los flags de la pauta mas `-g` para poder mirar los core si algo revienta.
compila sin un solo warning. `-pthread` aca si se usa: todo el programa son
hilos POSIX. `make` corre exactamente esa linea.

el detalle que hace falta: con `-std=c17` (que es ISO estricto) glibc esconde
las declaraciones POSIX, asi que los archivos que usan `sigwait`, `usleep` o
`sem_timedwait` arrancan con

```c
#define _DEFAULT_SOURCE
```

**antes del primer `#include`**. sin esa linea no compila: salen
`unknown type name 'sigset_t'`, `implicit declaration of function 'usleep'` o
`implicit declaration of function 'sem_timedwait'`.

## como se ejecuta

```sh
./torneo N M [config.txt]
```

- `N`: cantidad de jugadores. entran de a uno, cada 20 ms, y se emparejan por
  orden de llegada. el primero de cada pareja hace de puma.
- `M`: mesas simultaneas como maximo. si hay mas parejas que mesas, las demas
  esperan dormidas en un semaforo, sin gastar CPU. si `M` supera 1000 se recorta
  a 1000.
- `config.txt`: opcional. si no se pasa se usan los valores por defecto; si se
  pasa y no se puede leer, tambien, y se avisa por stderr.
- si `N` es impar, el ultimo jugador se queda sin rival y se informa en la cola.

cada partida termina por una de estas cuatro razones: el puma llega a 6 capturas,
los perros lo dejan sin movimientos, pasan 30 s, o se llega a 5000 movimientos.
los dos ultimos son topes para que los perros no se queden moviendose de ida y
vuelta para siempre.

## formato del config

una linea `CONFIG` con los campos en ese orden, y ademas las lineas opcionales,
que pueden ir en cualquier parte del archivo:

```
CONFIG SEED 42 DELAY_MIN 50 DELAY_MAX 300 PUMA_INIT 2 2
DESCONEXION_PCT 0
TIMEOUT_MS 1700
```

| Campo | Default | Que hace |
|---|---|---|
| `SEED` | `42` | semilla del `rand_r` de cada ficha; cambia los delays y las elecciones de movimiento |
| `DELAY_MIN` | `50` | minimo de ms que duerme una ficha antes de mover |
| `DELAY_MAX` | `300` | maximo de ms que duerme una ficha antes de mover |
| `PUMA_INIT f c` | `2 2` | casilla inicial del puma, fila y columna de 0 a 4 |
| `DESCONEXION_PCT` | `0` | porcentaje de movimientos en que una ficha se cae |
| `TIMEOUT_MS` | `0` | ms sin latido para declarar abandonada una ficha; `0` = `4 * DELAY_MAX + 500` |

se usa la **primera** linea `CONFIG` que traiga los cinco campos. si esa linea
trae valores fuera de rango (por ejemplo `DELAY_MAX` menor que `DELAY_MIN`, o un
puma fuera de 0..4), o si el archivo no tiene ninguna, se usan los valores por
defecto y se avisa. `DESCONEXION_PCT` y `TIMEOUT_MS` pueden ir en cualquier
linea; si aparecen dos veces gana la ultima. `TIMEOUT_MS 0` tambien significa
"se calcula solo".

los 12 perros no se configuran: arrancan en las 12 casillas del borde, en un
orden fijo, y se corren a la siguiente si el puma empieza justo en una de ellas.
`PUMA_INIT` mueve solo al puma.

`tests/test_cases.txt` trae los 12 casos de la pauta. de cada caso se lee solo la
linea `CONFIG`; las lineas `EVENT` todavia no se interpretan, los movimientos los
elige cada ficha al azar con la semilla. por eso se corre un caso por vez:

```sh
./torneo 2 1 tests/test_cases.txt
```

## lo que hay que mirar cuando corre

```
=== Torneo Komikan: 4 jugadores, 2 mesas ===
seed=42 delay=50-300 ms puma=2,2 desconexion=0% timeout=1700 ms
[Cola] jugador 1 espera rival
[Mesa 1] inicio (puma en 2,2, delay 50-300 ms)
[Cola] jugador 3 espera rival
[Mesa 2] inicio (puma en 2,2, delay 50-300 ms)
[Mesa 1] captura: puma 12 -> 10 (come 11), total 1
[Mesa 2] captura: puma 12 -> 14 (come 13), total 1
[Mesa 1] tablero a los 2018 ms (movimientos=119, capturas=4)
trono: . . .
. . o . . 
. . o . o 
. o . . o 
o . P o . 
. . . o . 
capturas: 4
[Mesa 1] fin: gana el puma (capturas=6, movimientos=439)
[Mesa 2] fin: gana el puma (capturas=6, movimientos=1378)
=== Servidor cerrado: 2 mesas atendidas (activas=0) ===
```

- **`[Cola]`** es el matchmaker: un jugador se anota y espera rival. cuando llega
  el segundo se arma la mesa.
- **`[Mesa k] captura`** se imprime cada vez que el puma come un perro. los demas
  movimientos no se imprimen, solo se cuentan.
- **`tablero a los ... ms`** es el tablero de esa mesa impreso cada 2 segundos,
  con los movimientos y las capturas que lleva. en el tablero, `P` es el puma,
  `o` un perro, `.` una casilla libre, y la primera linea son las tres casillas
  del trono.
- **`fin:`** dice quien gano y con cuantos movimientos, y despues imprime el
  tablero final.
- **`activas=0`** al cerrar es la prueba de que ninguna mesa quedo viva ni
  ningun cupo del semaforo sin devolver.

### el limite M en la practica

`M = 1` sale todo de a una partida. con `M = 2` o `M = 3` ya se ven varias mesas
escribiendo entrelazadas en la salida, y el log de cada una sale con su numero
de mesa adelante para no confundirlas.

con `./torneo 20 2` hay 10 partidas en tandas de 2. la corrida demora cerca de
70 s porque varias partidas llegan al tope de 30 s. no es un cuelgue: es el tope
funcionando.

el limite se cumple con un semaforo contador que arranca en M, mas un `assert`
de que `mesas_activas` nunca pase de M. una mesa libera su cupo con `sem_post`
recien cuando terminaron sus 13 hilos.

### Ctrl+C

con Ctrl+C el servidor no se muere de golpe. un hilo dedicado recibe la senal,
la bandera de cierre se prende, las mesas la ven en la proxima vuelta del juez,
cada una corta su partida, y el `main` espera a todos los hilos:

```
[SIGINT] cerrando el servidor...
[Mesa 1] fin: gana nadie (partida abortada) (capturas=4, movimientos=468)
...
=== Servidor cerrado: 2 mesas atendidas (activas=0) ===
```

el jugador que estaba esperando rival tambien se despierta y se va. no quedan
hilos ni mesas colgadas, y ASan no reporta fugas al cerrar.

### desconexiones

con `DESCONEXION_PCT` una ficha se va sin avisar despues de mover, y deja de
mandar latido. el juez revisa los latidos cada 50 ms; si pasan mas de
`TIMEOUT_MS`, marca la partida como abandonada y gana el rival:

```
[Mesa 1] ficha 0 se desconecta
[Mesa 1] abandono de la ficha 0
[Mesa 1] fin: gana los perros (capturas=1, movimientos=57)
```

si el que se cae es el puma, ganan los perros; si se cae un perro, gana el puma.

el caso mas feo es que la ficha se caiga **con el lock de la mesa tomado**. eso
no se puede simular con el config: esta en `./bin/paso8`, que hace que una ficha
haga `pthread_exit` adentro de la seccion critica. con el mutex robusto de la
mesa, el siguiente en tomar el lock recibe `EOWNERDEAD`, lo repara y aborta la
partida. con un mutex comun la mesa queda trabada para siempre.

## codigos de salida

| Codigo | Cuando |
|---|---|
| 0 | el torneo termino normalmente, incluso si se corto con Ctrl+C |
| 1 | uso incorrecto, `N` o `M` menores o iguales a 0, no se pudo crear el hilo de senales, o no hay memoria |

el resultado de cada partida no cambia el codigo de salida del torneo: una mesa
puede abortarse y el servidor igual termina en 0.

## pruebas

no hay un target `make test`: las pruebas son los binarios de cada paso y los
sanitizers. los pasos imprimen su propio veredicto, asi que se mira la ultima
linea.

```sh
make paso1 && ./bin/paso1     # tablero y reglas, con un test adentro
make paso2 && ./bin/paso2     # tiene que decir CARRERA DETECTADA
make paso3 && ./bin/paso3     # tiene que decir OK: no se perdio ninguna ficha
make paso3b && ./bin/paso3b   # lo mismo, con semaforo binario
make paso4 && ./bin/paso4     # una partida completa, termina con resultado: 0
make paso8 && ./bin/paso8     # el mutex robusto recupera la mesa
```

| Comando | Que prueba |
|---|---|
| `./bin/paso1` | tablero, saltos y reglas, con un test adentro |
| `./bin/paso2` | 13 hilos sin ningun lock: el tablero queda inconsistente |
| `./bin/paso3` | el mismo juego con un mutex por mesa: no se pierde nada |
| `./bin/paso3b` | lo mismo con semaforo binario, para comparar |
| `./bin/paso4` | una partida con juez, cond var y deteccion de victoria |
| `./bin/paso8` | una ficha muere con el lock tomado: `EOWNERDEAD` y se recupera |
| `./bin/paso8 normal` | el mismo caso con mutex comun: la mesa se cuelga y un reloj avisa a los 2 s |

los sanitizers se compilan aparte, con el nombre del archivo en `P`:

```sh
make tsan P=paso2     # bin/paso2-tsan
make tsan P=paso4     # bin/paso4-tsan
make tsan P=torneo    # bin/torneo-tsan
make asan P=paso4     # bin/paso4-asan
make asan P=torneo    # bin/torneo-asan
```

`paso2` bajo TSan tiene que mostrar las carreras (es el punto del paso). `paso3`,
`paso4` y `torneo` dan 0 warnings de TSan; `paso4` y `torneo` dan 0 errores de
ASan/UBSan.

el Ctrl+C hay que probarlo a mano, porque no se puede automatizar desde el
Makefile:

```sh
./torneo 20 2      # en otra terminal, Ctrl+C
pgrep torneo       # no debe imprimir nada
```

## los pasos

cada `pasoN.c` es el estado del programa despues de esa pieza. sirven para
mostrar el orden en que se construyo y para depurar por partes; para entregar
basta `torneo`.

| Paso | Que se agrega |
|---|---|
| `src/paso1.c` | el tablero, los 70 saltos y las reglas del komikan |
| `src/paso2.c` | 13 hilos moviendo sin ningun lock: la carrera, a proposito |
| `src/paso3.c` | un mutex por mesa, con validar y mover bajo el mismo lock |
| `src/paso3b.c` | el paso 3 con semaforo binario, para comparar en el informe |
| `src/paso4.c` | el juez: cond var, capturas, victoria y fin de partida |
| `src/paso5.c` | matchmaker de N jugadores y limite de M mesas |
| `src/paso6.c` | parser de `config.txt` |
| `src/paso7.c` | SIGINT con `sigwait` y cierre ordenado |
| `src/paso8.c` | mutex robusto: una ficha muere con el lock tomado |

`paso3b` esta aparte a proposito: el semaforo binario sirve como exclusion mutua
y el paso 3 pasa igual, pero un semaforo no tiene forma de avisar que su duenio
murio. por eso el torneo usa mutex robusto y no semaforo para el tablero.

## limitaciones conocidas

- la posicion inicial de los 12 perros es fija; el config solo mueve al puma.
- del archivo de casos se lee la primera linea `CONFIG` valida; las lineas
  `EVENT` no se interpretan.
- una partida corta a los 30 s o 5000 movimientos, y ese final se informa como
  `gana nadie (tope de tiempo)`.
- `M` se recorta a 1000 mesas (`MAX_HILOS_MESA`).
- ThreadSanitizer no entiende los mutex robustos: en `paso8` puede marcar un
  warning en `mesa_lock` que es un falso positivo. en `paso3`, `paso4` y
  `torneo` da 0.
- `src/servidor.c` quedo de una version anterior y el Makefile no lo compila; el
  entregable es `torneo`.

## decisiones de diseno

- **un mutex por mesa, no uno global.** cada partida protege su propio tablero,
  asi una mesa lenta no frena a las demas. lo unico compartido entre mesas es el
  estado del matchmaker, que tiene su propio mutex.
- **el lock cubre elegir, validar y mover.** si solo protegiera la escritura, dos
  fichas podrian validar la misma casilla libre antes de que ninguna escriba, y
  mudarse las dos. `tablero_mover` y `tablero_capturar` validan y aplican en la
  misma llamada, siempre con el lock tomado.
- **la ficha duerme afuera del lock.** el delay aleatorio se cumple antes de
  entrar a la seccion critica. si durmiera adentro, una ficha lenta trabaria a las
  otras 12.
- **cond var con `pthread_cond_timedwait`.** el juez duerme hasta que una ficha
  avisa con `pthread_cond_signal`, y ademas se despierta cada 50 ms para revisar
  los latidos, el tope de tiempo y la bandera de Ctrl+C. por eso el `while` y no
  un `if`: el wait puede volver sin que nadie haya hecho signal.
- **mutex robusto en la mesa.** si una ficha muere con el lock tomado, el
  siguiente `lock` recibe `EOWNERDEAD`: se llama a `pthread_mutex_consistent`,
  se marca la partida como abortada y se suelta el lock. sin eso la mesa queda
  muerta para siempre.
- **semaforo contador para las mesas.** `sem_mesas` arranca en M: son cupos, no
  exclusion mutua. la pareja que llega cuando no hay mesa espera con
  `sem_timedwait`, que duerme en el kernel y no gasta CPU; el timeout de 100 ms
  es solo para poder mirar si el servidor se esta cerrando.
- **`sigwait` en un hilo aparte.** un handler de senal no puede tomar locks, asi
  que SIGINT se bloquea en todos los hilos y un hilo dedicado la espera con
  `sigwait`. cuando vuelve, ya estamos en contexto normal y si se puede usar el
  mutex y el cond.
- **latidos en vez de avisos de desconexion.** cada ficha actualiza su
  `latido[id]` en cada vuelta, con el lock tomado. el juez compara contra
  `timeout_ms`. asi una ficha que se cae sin avisar se detecta igual que una que
  avisa, y no hace falta que el hilo alcance a despedirse.
- **orden de locks: primero la mesa, despues el log.** nunca al reves, para que
  dos mesas no se queden esperando la una a la otra.
- **un solo lock para el log.** las lineas de distintas mesas se imprimen
  enteras, sin mezclarse a mitad de renglon.
- **el tablero es un grafo, no una matriz de vecinos.** los 70 saltos legales
  (60 de la grilla y 10 del trono) se arman una sola vez con `pthread_once`, y
  moverse es buscar el salto en esa tabla. validar adyacencia y validar captura
  usan la misma tabla, asi no pueden quedar desincronizadas.
- **tope de movimientos y de tiempo por partida.** corta el livelock de los
  perros moviendose de ida y vuelta sin progreso, que si no dejaria la mesa
  ocupada para siempre.

## archivos

| Ruta | Que es |
|---|---|
| `src/torneo.c` | el entregable: matchmaker, mesas y SIGINT |
| `src/mesa.c`, `src/mesa.h` | la partida: 13 hilos, juez, latidos y fin |
| `src/tablero.c`, `src/tablero.h` | el tablero como grafo, los saltos y las reglas |
| `src/config.c`, `src/config.h` | el parser de `config.txt` |
| `src/paso1.c` ... `src/paso8.c` | la construccion paso a paso |
| `tests/test_cases.txt` | los 12 casos de la pauta |
| `docs/T2_os_2026_02.pdf` | el enunciado |
| `Makefile` | `all`, `pasoN`, `tsan`, `asan`, `clean` |
