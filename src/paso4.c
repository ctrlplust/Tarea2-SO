// Paso 4: una partida completa. El juez crea las 13 fichas, espera en una
// cond var, detecta la victoria y hace join. Corre una sola mesa.

#include <stdio.h>

#include "mesa.h"

int main(void) {
    Config cfg = {
        .seed = 42,
        .delay_min = 1,
        .delay_max = 10,
        .puma_fila = 2,
        .puma_col = 2,
        .desconexion_pct = 0,
        .timeout_ms = 2000,
    };

    int ganador = jugar_partida(1, &cfg, NULL);
    printf("resultado: %d\n", ganador);
    return 0;
}
