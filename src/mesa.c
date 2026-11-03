// Mesa de juego: 13 hilos (1 puma + 12 perros) y el juez, que es quien llama
// a jugar_partida().

#define _DEFAULT_SOURCE

#include "mesa.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tablero.h"

#define CAPTURAS_VICTORIA  6
#define MAX_MOVIMIENTOS 5000
#define MAX_MS         30000
#define PERIODO_JUEZ_MS   50
#define PERIODO_IMPRESION_MS 2000   // cada cuanto el juez imprime el tablero

// Un solo lock para el log, asi las lineas de distintas mesas no se mezclan.
// Orden de locks: primero el mutex de la mesa y despues este. Nunca al reves,
// si no dos mesas se pueden quedar trabadas.
static pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    Tablero t;
    pthread_mutex_t m;
    pthread_cond_t cambio;
    int terminada;
    int ganador;                 // 0 puma, 1 perros, 2 tope, -1 abortada
    int movimientos;
    long long inicio_ms;
    long long latido[1 + N_PERROS];   // ultima vez que se movio cada ficha
    int id_mesa;
    int id_que_muere;            // paso 8: ficha que se cae con el lock tomado
    const Config *cfg;
} Mesa;

typedef struct {
    Mesa *mesa;
    int id;                      // 0 = puma, 1..12 = perros
    int pos;
    bool es_puma;
    unsigned seed;
} Ficha;

static long long ahora_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// cond_timedwait usa CLOCK_REALTIME por defecto, no el monotono
static struct timespec ts_en_ms(long ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return ts;
}

static void log_mesa(int id_mesa, const char *fmt, ...) {
    va_list ap;
    pthread_mutex_lock(&mutex_log);
    printf("[Mesa %d] ", id_mesa);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&mutex_log);
}

// El mutex de la mesa es "robusto": si el hilo que lo tiene se muere, el
// siguiente lock no se cuelga, recibe EOWNERDEAD. Cuando pasa hay que hacer
// tres cosas, en este orden:
//   1. pthread_mutex_consistent, si no el mutex queda roto para siempre.
//   2. marcar la partida como abortada, porque el tablero ya no es confiable.
//   3. soltar el lock y devolver -1.
// Lo de soltar es importante: el lock ya lo tomamos nosotros. Si volvieramos
// sin soltarlo, el siguiente que entre recibe EOWNERDEAD de nuevo y se arma
// una cadena de hilos muriendo uno atras del otro.
static int mesa_lock(Mesa *m) {
    int rc = pthread_mutex_lock(&m->m);
    if (rc == EOWNERDEAD) {
        pthread_mutex_consistent(&m->m);
        m->terminada = 1;
        m->ganador = -1;
        log_mesa(m->id_mesa, "EOWNERDEAD: el dueno del lock murio, se recupera");
        pthread_cond_broadcast(&m->cambio);
        pthread_mutex_unlock(&m->m);
        return -1;
    }
    return rc == 0 ? 0 : -1;
}

static bool intentar_movimiento(Mesa *m, Ficha *f) {
    int vecinos[MAX_VEC];
    int n = vecinos_de(f->pos, vecinos);
    int libres[MAX_VEC], nl = 0;
    for (int i = 0; i < n; i++)
        if (m->t.casilla[vecinos[i]] == LIBRE)
            libres[nl++] = vecinos[i];
    if (nl == 0)
        return false;

    // elige una de las libres al azar y deja que tablero_mover valide de nuevo
    int destino = libres[rand_r(&f->seed) % (unsigned)nl];
    if (!tablero_mover(&m->t, f->pos, destino))
        return false;
    f->pos = destino;
    return true;
}

static bool intentar_captura(Mesa *m, Ficha *f) {
    Salto lista[MAX_SALTOS];
    int n = saltos_desde(f->pos, lista, MAX_SALTOS);
    int validos[MAX_SALTOS], nv = 0;
    for (int i = 0; i < n; i++)
        if (es_captura_valida(&m->t, f->pos, lista[i].com, lista[i].fin))
            validos[nv++] = i;
    if (nv == 0)
        return false;

    int k = validos[rand_r(&f->seed) % (unsigned)nv];
    int comido = lista[k].com, fin = lista[k].fin;
    if (!tablero_capturar(&m->t, f->pos, comido, fin))
        return false;
    log_mesa(m->id_mesa, "captura: puma %d -> %d (come %d), total %d",
             f->pos, fin, comido, m->t.capturas);
    f->pos = fin;
    return true;
}

static void *hilo_ficha(void *arg) {
    Ficha *f = arg;
    Mesa *m = f->mesa;

    for (;;) {
        int rango = m->cfg->delay_max - m->cfg->delay_min + 1;
        int ms = m->cfg->delay_min;
        if (rango > 1)
            ms += (int)(rand_r(&f->seed) % (unsigned)rango);
        usleep((useconds_t)ms * 1000);          // dormir siempre afuera del lock

        if (mesa_lock(m) != 0)
            return NULL;                        // el dueno anterior murio con el lock
        if (m->terminada) {
            pthread_mutex_unlock(&m->m);
            break;
        }

        m->latido[f->id] = ahora_ms();          // "sigo vivo"

        // Paso 8: esta ficha se cae en el medio, con el lock tomado y sin
        // hacer unlock. Es como si el proceso del jugador se muriera de golpe.
        // El mutex robusto es el que evita que la mesa quede trabada.
        // Espera a que haya unos movimientos para que se vea una partida en
        // curso y no una mesa recien abierta.
        if (f->id == m->id_que_muere && m->movimientos >= 20) {
            log_mesa(m->id_mesa, "ficha %d se cae CON EL LOCK TOMADO", f->id);
            pthread_exit(NULL);
        }

        bool movio = false;
        if (f->es_puma)
            movio = intentar_captura(m, f);     // el puma primero intenta comer
        if (!movio)
            movio = intentar_movimiento(m, f);
        if (movio) {
            m->movimientos++;
            pthread_cond_signal(&m->cambio);    // despierta al juez
        }

        pthread_mutex_unlock(&m->m);

        // desconexion simulada: el hilo se va sin avisar y deja de latir
        if (m->cfg->desconexion_pct > 0 &&
            (int)(rand_r(&f->seed) % 100) < m->cfg->desconexion_pct) {
            log_mesa(m->id_mesa, "ficha %d se desconecta", f->id);
            return NULL;
        }
    }
    return NULL;
}

static const char *nombre_ganador(int g) {
    switch (g) {
    case 0:  return "el puma";
    case 1:  return "los perros";
    case 2:  return "nadie (tope de tiempo)";
    default: return "nadie (partida abortada)";
    }
}

// Las tres versiones publicas de abajo son atajos de esta. `robusto` decide si
// el mutex de la mesa se crea con PTHREAD_MUTEX_ROBUST o como un mutex comun.
static int jugar_interna(int id_mesa, const Config *cfg, int (*abortar)(void),
                         int id_que_muere, bool robusto);

int jugar_partida(int id_mesa, const Config *cfg, int (*abortar)(void)) {
    return jugar_interna(id_mesa, cfg, abortar, -1, true);
}

int jugar_partida_con_fallo(int id_mesa, const Config *cfg,
                            int (*abortar)(void), int id_que_muere) {
    return jugar_interna(id_mesa, cfg, abortar, id_que_muere, true);
}

int jugar_partida_sin_robusto(int id_mesa, const Config *cfg,
                              int (*abortar)(void), int id_que_muere) {
    return jugar_interna(id_mesa, cfg, abortar, id_que_muere, false);
}

static int jugar_interna(int id_mesa, const Config *cfg, int (*abortar)(void),
                         int id_que_muere, bool robusto) {
    Mesa m;
    memset(&m, 0, sizeof m);
    m.id_mesa = id_mesa;
    m.id_que_muere = id_que_muere;
    m.cfg = cfg;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (robusto)
        pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&m.m, &attr);
    pthread_mutexattr_destroy(&attr);
    pthread_cond_init(&m.cambio, NULL);

    tablero_iniciar(&m.t, cfg->puma_fila, cfg->puma_col);

    // Las 13 fichas se arman antes de crear los hilos. Si se arman despues, el
    // puma ya se estaria moviendo mientras el juez lee el tablero.
    Ficha fichas[1 + N_PERROS];
    pthread_t hilos[1 + N_PERROS];

    // La semilla de cada ficha mezcla la del config con la mesa y el id, para
    // que dos mesas con el mismo config no jueguen la misma partida.
    unsigned base = cfg->seed ^ (unsigned)id_mesa * 2654435761u;
    fichas[0] = (Ficha){&m, 0, m.t.puma, true, base ^ 0x9e3779b9u};
    int k = 1;
    for (int n = 0; n < N_CELDAS; n++) {
        if (m.t.casilla[n] != PERRO)
            continue;
        fichas[k] = (Ficha){&m, k, n, false, base + 7919u * (unsigned)k};
        k++;
    }
    if (k != 1 + N_PERROS) {
        log_mesa(id_mesa, "error: se armaron %d fichas", k);
        pthread_cond_destroy(&m.cambio);
        pthread_mutex_destroy(&m.m);
        return -1;
    }

    m.inicio_ms = ahora_ms();
    for (int i = 0; i < 1 + N_PERROS; i++)
        m.latido[i] = m.inicio_ms;

    for (int i = 0; i < 1 + N_PERROS; i++)
        pthread_create(&hilos[i], NULL, hilo_ficha, &fichas[i]);

    log_mesa(id_mesa, "inicio (puma en %d,%d, delay %d-%d ms)",
             cfg->puma_fila, cfg->puma_col, cfg->delay_min, cfg->delay_max);

    // --- bucle del juez ---
    if (mesa_lock(&m) == 0) {
        long long ultima_impresion = ahora_ms();

        // while y no if: el cond_wait puede despertar sin que nadie haya hecho
        // signal, asi que hay que volver a mirar la condicion
        while (!m.terminada) {
            struct timespec ts = ts_en_ms(PERIODO_JUEZ_MS);
            int rc = pthread_cond_timedwait(&m.cambio, &m.m, &ts);

            // el timedwait es para despertarse igual cada 50 ms: asi el juez
            // revisa desconexiones y Ctrl+C aunque nadie se mueva
            if (rc == EOWNERDEAD) {             // una ficha murio con el lock
                pthread_mutex_consistent(&m.m);
                log_mesa(id_mesa, "EOWNERDEAD en el juez: el lock se recupera");
                m.terminada = 1;
                m.ganador = -1;
                break;
            }
            if (abortar && abortar()) {
                m.terminada = 1;
                m.ganador = -1;
            } else if (m.t.capturas >= CAPTURAS_VICTORIA) {
                m.terminada = 1;
                m.ganador = 0;                  // gana el puma
            } else if (puma_sin_movimientos(&m.t)) {
                m.terminada = 1;
                m.ganador = 1;                  // gana el cerco de perros
            } else if (m.movimientos >= MAX_MOVIMIENTOS ||
                       ahora_ms() - m.inicio_ms >= MAX_MS) {
                m.terminada = 1;
                m.ganador = 2;                  // tope, para no quedar en livelock
            } else {
                long long ahora = ahora_ms();
                for (int i = 0; i < 1 + N_PERROS; i++) {
                    if (ahora - m.latido[i] > cfg->timeout_ms) {
                        log_mesa(id_mesa, "abandono de la ficha %d", i);
                        m.terminada = 1;
                        m.ganador = (i == 0) ? 1 : 0;   // gana el rival
                        break;
                    }
                }
            }

            // El enunciado pide ver el tablero mientras se juega, no solo al
            // final. Se imprime cada 2 s y no en cada movimiento porque con 13
            // fichas cada 50-300 ms seria una muralla de texto.
            long long ahora_print = ahora_ms();
            if (!m.terminada &&
                ahora_print - ultima_impresion >= PERIODO_IMPRESION_MS) {
                pthread_mutex_lock(&mutex_log);
                printf("[Mesa %d] tablero a los %lld ms (movimientos=%d, capturas=%d)\n",
                       id_mesa, ahora_print - m.inicio_ms, m.movimientos,
                       m.t.capturas);
                tablero_imprimir(&m.t);
                fflush(stdout);
                pthread_mutex_unlock(&mutex_log);
                ultima_impresion = ahora_print;
            }
        }
        pthread_mutex_unlock(&m.m);
    }
    // Si mesa_lock devolvio -1 no hay nada que hacer aca: ya dejo terminada=1
    // y ganador=-1 con el lock tomado, y las fichas van a salir solas.

    for (int i = 0; i < 1 + N_PERROS; i++)
        pthread_join(hilos[i], NULL);

    pthread_mutex_lock(&mutex_log);
    printf("[Mesa %d] fin: gana %s (capturas=%d, movimientos=%d)\n",
           id_mesa, nombre_ganador(m.ganador), m.t.capturas, m.movimientos);
    tablero_imprimir(&m.t);
    fflush(stdout);
    pthread_mutex_unlock(&mutex_log);

    pthread_cond_destroy(&m.cambio);
    pthread_mutex_destroy(&m.m);
    return m.ganador;
}
