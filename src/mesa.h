#ifndef MESA_H
#define MESA_H

// Parametros que necesita una mesa para jugar una partida.
typedef struct {
    unsigned seed;
    int delay_min;        // ms entre movimientos
    int delay_max;
    int puma_fila;        // posicion inicial del puma en la grilla
    int puma_col;
    int desconexion_pct;  // probabilidad por movimiento de que una ficha se caiga
    int timeout_ms;       // si una ficha no late en este tiempo, se considera caida
} Config;

// Juega una partida completa y bloquea hasta el final.
// Devuelve 0 si gana el puma, 1 si ganan los perros, 2 si se llego al limite
// de tiempo/movimientos y -1 si la partida se aborto.
// abortar: si no es NULL se consulta en cada vuelta del juez (para SIGINT).
int jugar_partida(int id_mesa, const Config *cfg, int (*abortar)(void));

#endif
