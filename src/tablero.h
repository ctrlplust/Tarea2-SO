#ifndef TABLERO_H
#define TABLERO_H

#include <stdbool.h>

// Nodos 0-24: grilla 5x5, id = fila*5 + col.
// 25=I, 26=T, 27=D: los tres vertices del trono.
#define N_NODOS    28
#define N_CELDAS   25
#define N_PERROS   12
#define MAX_VEC     4
#define MAX_SALTOS 70

enum Ficha { LIBRE, PUMA, PERRO };

typedef struct {
    int ini, com, fin;   // de ini salta sobre com (el que se come) y cae en fin
} Salto;

typedef struct {
    enum Ficha casilla[N_NODOS];
    int puma;            // nodo donde esta el puma, para no buscarlo cada vez
    int capturas;
} Tablero;

// Arma adj[] y saltos[]. Se puede llamar desde varios hilos: adentro usa
// pthread_once, asi que la geometria se construye una sola vez.
void tablero_geometria_init(void);

// Pone el puma en (fila, col) y reparte los 12 perros.
void tablero_iniciar(Tablero *t, int fila, int col);
void tablero_imprimir(const Tablero *t);

int  nodo_de(int fila, int col);
bool son_vecinos(int a, int b);
int  vecinos_de(int nodo, int *salida);              // llena salida y devuelve cuantos son
int  saltos_desde(int nodo, Salto *salida, int max); // idem, con los saltos que salen de nodo

bool es_movimiento_valido(const Tablero *t, int desde, int hasta);
bool es_captura_valida(const Tablero *t, int desde, int sobre, int hasta);
bool puma_sin_movimientos(const Tablero *t);

// Validan y aplican de una. Hay que llamarlas con el lock de la mesa tomado,
// si no dos hilos pueden validar la misma casilla libre y mudarse los dos.
bool tablero_mover(Tablero *t, int desde, int hasta);
bool tablero_capturar(Tablero *t, int desde, int sobre, int hasta);

int  tablero_contar(const Tablero *t, enum Ficha ficha);

#endif
