// Paso 2: una partida con 13 hilos y SIN sincronizacion, a proposito.
// Cada ficha decide su movimiento leyendo el tablero, duerme y despues actua
// sin revalidar: entre la decision y la escritura otro hilo puede ocupar la
// misma casilla. Compilar con -fsanitize=thread para ver el reporte.

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

typedef struct {
    int id;
    int pos;          // posicion que el hilo cree tener
    bool es_puma;
    unsigned seed;    // rand_r por hilo: rand() no es thread-safe
} Ficha;

// elige un vecino libre leyendo el tablero SIN lock: la decision puede quedar vieja
static int elegir_destino(int pos, unsigned *seed) {
    int vecinos[MAX_VEC];
    int n = vecinos_de(pos, vecinos);
    int libres[MAX_VEC], m = 0;
    for (int i = 0; i < n; i++)
        if (tablero.casilla[vecinos[i]] == LIBRE)
            libres[m++] = vecinos[i];
    if (m == 0)
        return -1;
    return libres[rand_r(seed) % (unsigned)m];
}

static void *hilo_ficha(void *arg) {
    Ficha *f = arg;
    for (int i = 0; i < ITERACIONES; i++) {
        int destino = elegir_destino(f->pos, &f->seed);
        int ms = DELAY_MIN_MS + (int)(rand_r(&f->seed) % (DELAY_MAX_MS - DELAY_MIN_MS + 1));
        usleep((useconds_t)ms * 1000);
        if (destino < 0)
            continue;
        // actua sin revalidar: aca esta la carrera
        tablero.casilla[f->pos] = LIBRE;
        tablero.casilla[destino] = f->es_puma ? PUMA : PERRO;
        f->pos = destino;
    }
    return NULL;
}

int main(void) {
    tablero_geometria_init();
    tablero_iniciar(&tablero, 2, 2);

    Ficha fichas[1 + N_PERROS];
    pthread_t hilos[1 + N_PERROS];

    // armar todas las fichas ANTES de crear hilos: si no, el puma ya se mueve
    // mientras main todavia lee el tablero para ubicar a los perros
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

    printf("\n=== Fin del paso 2 (sin sincronizacion) ===\n");
    tablero_imprimir(&tablero);

    int pumas = tablero_contar(&tablero, PUMA);
    int perros = tablero_contar(&tablero, PERRO);
    int desinc = 0;
    for (int i = 0; i < 1 + N_PERROS; i++) {
        enum Ficha esperada = fichas[i].es_puma ? PUMA : PERRO;
        if (tablero.casilla[fichas[i].pos] != esperada)
            desinc++;
    }

    printf("pumas en el tablero: %d (deberia ser 1)\n", pumas);
    printf("perros en el tablero: %d (deberian ser %d)\n", perros, N_PERROS);
    printf("fichas que no estan donde creen: %d de %d\n", desinc, 1 + N_PERROS);

    if (pumas != 1 || perros != N_PERROS || desinc > 0)
        printf("CARRERA DETECTADA: el tablero quedo inconsistente\n");
    else
        printf("esta corrida no mostro la carrera; repeti con delay 1 ms\n");

    return 0;
}
