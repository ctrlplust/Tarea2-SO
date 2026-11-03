// Paso 3b: igual que el paso 3 pero el lock es un semaforo binario.
// Sirve para comparar con el mutex en el informe.

#define _DEFAULT_SOURCE

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tablero.h"

#define ITERACIONES  300
#define DELAY_MIN_MS   1
#define DELAY_MAX_MS   3

static Tablero tablero;
static sem_t lock;          // 1 = libre, 0 = tomado (semaforo binario)
static int movimientos;

typedef struct {
    int id;
    int pos;
    bool es_puma;
    unsigned seed;
} Ficha;

// sem_wait puede volver con EINTR si llega una senal; ahi hay que reintentar
static void tomar(void) {
    int rc;
    do {
        rc = sem_wait(&lock);
    } while (rc != 0 && errno == EINTR);
}

static void soltar(void) {
    sem_post(&lock);
}

static void *hilo_ficha(void *arg) {
    Ficha *f = arg;
    for (int i = 0; i < ITERACIONES; i++) {
        int ms = DELAY_MIN_MS + (int)(rand_r(&f->seed) % (DELAY_MAX_MS - DELAY_MIN_MS + 1));
        usleep((useconds_t)ms * 1000);

        tomar();

        // mismo bloque que el paso 3: elegir, validar y mover con el lock tomado
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

        soltar();
    }
    return NULL;
}

int main(void) {
    tablero_geometria_init();
    tablero_iniciar(&tablero, 2, 2);
    sem_init(&lock, 0, 1);

    Ficha fichas[1 + N_PERROS];
    pthread_t hilos[1 + N_PERROS];

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

    printf("\n=== Fin del paso 3b (semaforo binario) ===\n");
    tablero_imprimir(&tablero);

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

    sem_destroy(&lock);

    if (pumas == 1 && perros == N_PERROS && desinc == 0) {
        printf("OK: no se perdio ninguna ficha\n");
        return 0;
    }
    printf("FALLO: el tablero quedo inconsistente\n");
    return 1;
}
