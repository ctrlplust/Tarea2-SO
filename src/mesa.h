#ifndef MESA_H
#define MESA_H

// Lo que necesita una mesa para jugar una partida. Los delays van en ms.
typedef struct {
    unsigned seed;
    int delay_min;
    int delay_max;
    int puma_fila;        // posicion inicial del puma en la grilla
    int puma_col;
    int desconexion_pct;  // % de movimientos en que una ficha se cae
    int timeout_ms;       // ms sin latido para declarar abandono
} Config;

// Juega una partida y no vuelve hasta que termina.
// Devuelve 0 si gana el puma, 1 si ganan los perros, 2 si se llego al tope de
// tiempo o movimientos, y -1 si se aborto.
// abortar: se llama en cada vuelta del juez. El torneo la usa para cortar con
// Ctrl+C; el paso 4 le pasa NULL.
int jugar_partida(int id_mesa, const Config *cfg, int (*abortar)(void));

// Igual que jugar_partida, pero la ficha `id_que_muere` (0 = puma, 1..12 =
// perros, -1 = ninguna) se muere con el lock de la mesa tomado, cuando la
// partida ya lleva unos movimientos. Sirve para el paso 8: probar que el mutex
// robusto se recupera y la mesa no queda muerta.
int jugar_partida_con_fallo(int id_mesa, const Config *cfg,
                            int (*abortar)(void), int id_que_muere);

// Lo mismo pero con un mutex comun, sin robusto. Si la ficha se muere con el
// lock tomado la mesa queda trabada para siempre. Es solo para la comparacion
// del paso 8: el torneo nunca usa esta.
int jugar_partida_sin_robusto(int id_mesa, const Config *cfg,
                              int (*abortar)(void), int id_que_muere);

#endif
