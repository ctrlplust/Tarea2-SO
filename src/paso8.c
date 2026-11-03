// Paso 8: que pasa si una ficha se muere con el lock de la mesa tomado.
//
// La ficha hace pthread_exit adentro de la seccion critica, sin unlock. Es lo
// mismo que si el proceso de ese jugador se cayera de golpe.
//
//   ./bin/paso8           mutex robusto: el que entre despues recibe
//                         EOWNERDEAD, lo arregla y la partida se aborta.
//   ./bin/paso8 normal    mutex comun: nadie puede recuperar el lock y la
//                         mesa queda trabada para siempre.
//
// Con argumento numerico se elige que ficha se cae: 0 = puma, 1..12 = perros.

#define _DEFAULT_SOURCE

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "mesa.h"

// Solo para el modo "normal": si la mesa se cuelga, este hilo avisa y mata el
// proceso a los 2 segundos. En el torneo no existe nada de esto.
static atomic_int demo_terminada = 0;

static void *hilo_reloj(void *arg) {
    (void)arg;
    for (int i = 0; i < 20; i++) {                  // 20 * 100 ms = 2 s
        struct timespec ts = {0, 100 * 1000000L};
        nanosleep(&ts, NULL);
        if (atomic_load(&demo_terminada))
            return NULL;
    }
    printf("\n[reloj] pasaron 2 s y la mesa no responde: quedo colgada\n");
    printf("[reloj] con mutex comun no hay EOWNERDEAD, nadie puede recuperar el lock\n");
    fflush(stdout);
    _exit(0);           // se mata el proceso para no dejar la demo trabada
}

int main(int argc, char *argv[]) {
    bool robusto = true;
    int id_que_muere = 3;

    if (argc > 1) {
        if (strcmp(argv[1], "normal") == 0)
            robusto = false;
        else
            id_que_muere = atoi(argv[1]);
    }
    if (id_que_muere < 0 || id_que_muere > 12) {
        fprintf(stderr, "Uso: %s [normal | id_de_ficha 0..12]\n", argv[0]);
        return 1;
    }

    Config cfg = {
        .seed = 42,
        .delay_min = 1,
        .delay_max = 5,
        .puma_fila = 2,
        .puma_col = 2,
        .desconexion_pct = 0,
        .timeout_ms = 2000,
    };

    printf("=== Paso 8: la ficha %d se cae con el lock tomado (%s) ===\n",
           id_que_muere, robusto ? "mutex robusto" : "mutex comun");

    int ganador;
    if (robusto) {
        ganador = jugar_partida_con_fallo(1, &cfg, NULL, id_que_muere);
    } else {
        pthread_t reloj;
        pthread_create(&reloj, NULL, hilo_reloj, NULL);
        ganador = jugar_partida_sin_robusto(1, &cfg, NULL, id_que_muere);
        atomic_store(&demo_terminada, 1);   // no hizo falta el reloj
    }

    printf("resultado: %d (-1 = partida abortada, el lock se recupero)\n", ganador);
    return ganador == -1 ? 0 : 1;
}
