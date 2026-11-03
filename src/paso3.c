// Paso 3: el mismo juego que el paso 2, pero con un mutex por mesa.

#define _DEFAULT_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tablero.h"

#define ITERACIONES  300
#define DELAY_MIN_MS   1
#define DELAY_MAX_MS   3

static Tablero tablero;
static pthread_mutex_t mutex_tablero = PTHREAD_MUTEX_INITIALIZER;
static int movimientos;

typedef struct {
    int id;
    int pos;
    bool es_puma;
    unsigned seed;
} Ficha;

static void *hilo_ficha(void *arg) {
    Ficha *f = arg;
    for (int i = 0; i < ITERACIONES; i++) {
        int ms = DELAY_MIN_MS + (int)(rand_r(&f->seed) % (DELAY_MAX_MS - DELAY_MIN_MS + 1));
        usleep((useconds_t)ms * 1000);   // dormir va afuera del lock

        pthread_mutex_lock(&mutex_tablero);

        // Lo importante: el lock cubre elegir, validar y mover. Si solo
        // protegioramos la escritura, dos hilos podrian validar la misma
        // casilla libre antes de que ninguno escriba, y mudarse los dos.
        int vecinos[MAX_VEC];
        int n = vecinos_de(f->pos, vecinos);
        int libres[MAX_VEC], m = 0;
        for (int k = 0; k < n; k++)
            if (tablero.casilla[vecinos[k]] == LIBRE)
                libres[m++] = vecinos[k];
        if (m > 0) {
            int destino = libres[rand_r(&f->seed) % (unsigned)m];
            if (tablero_mover(&tablero, f->pos, destino)) {
                f->pos = destino;
                movimientos++;
            }
        }

        pthread_mutex_unlock(&mutex_tablero);
    }
    return NULL;
}

int main(void) {
    tablero_geometria_init();
    tablero_iniciar(&tablero, 2, 2);

    Ficha fichas[1 + N_PERROS];
    pthread_t hilos[1 + N_PERROS];

    // las 13 fichas se arman antes de crear hilos: si no, el puma ya se mueve
    // mientras main lee el tablero
    fichas[0] = (Ficha){0, tablero.puma, true, 42u};

    int k = 1;
    for (int n = 0; n < N_CELDAS; n++) {
        if (tablero.casilla[n] != PERRO)
            continue;
        fichas[k] = (Ficha){k, n, false, 1000u + (unsigned)k};
        k++;
    }
    if (k != 1 + N_PERROS) {
        fprintf(stderr, "error: se crearon %d fichas\n", k);
        return 1;
    }

    for (int i = 0; i < 1 + N_PERROS; i++)
        pthread_create(&hilos[i], NULL, hilo_ficha, &fichas[i]);

    for (int i = 0; i < 1 + N_PERROS; i++)
        pthread_join(hilos[i], NULL);

    printf("\n=== Fin del paso 3 (mutex por mesa) ===\n");
    tablero_imprimir(&tablero);

    // con el lock bien puesto no se pierde ninguna ficha
    int pumas = tablero_contar(&tablero, PUMA);
    int perros = tablero_contar(&tablero, PERRO);
    int desinc = 0;
    for (int i = 0; i < 1 + N_PERROS; i++) {
        enum Ficha esperada = fichas[i].es_puma ? PUMA : PERRO;
        if (tablero.casilla[fichas[i].pos] != esperada)
            desinc++;
    }

    printf("movimientos aplicados: %d\n", movimientos);
    printf("pumas en el tablero: %d (deberia ser 1)\n", pumas);
    printf("perros en el tablero: %d (deberian ser %d)\n", perros, N_PERROS);
    printf("fichas que no estan donde creen: %d de %d\n", desinc, 1 + N_PERROS);

    pthread_mutex_destroy(&mutex_tablero);

    if (pumas == 1 && perros == N_PERROS && desinc == 0) {
        printf("OK: no se perdio ninguna ficha\n");
        return 0;
    }
    printf("FALLO: el tablero quedo inconsistente\n");
    return 1;
}
