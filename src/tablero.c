// Tablero de Komikan como grafo. Es el paso 1 convertido en modulo para que
// los pasos con hilos no repitan la geometria.

#include "tablero.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static int adj[N_NODOS][MAX_VEC];
static int n_adj[N_NODOS];
static Salto saltos[MAX_SALTOS];
static int n_saltos;
static pthread_once_t geometria_once = PTHREAD_ONCE_INIT;

static void agregar_vecino(int a, int b) {
    assert(n_adj[a] < MAX_VEC);
    adj[a][n_adj[a]++] = b;
}

static void agregar_arista(int a, int b) {
    agregar_vecino(a, b);
    agregar_vecino(b, a);
}

static void agregar_salto(int ini, int com, int fin) {
    assert(n_saltos < MAX_SALTOS);
    saltos[n_saltos++] = (Salto){ini, com, fin};
}

// el del medio no cambia: se agrega el salto de ida y el de vuelta
static void agregar_salto_doble(int a, int b, int c) {
    agregar_salto(a, b, c);
    agregar_salto(c, b, a);
}

static void construir_grilla(void) {
    int df[4] = {-1, 1, 0, 0};   // arriba, abajo, izq, der
    int dc[4] = { 0, 0,-1, 1};
    for (int id = 0; id < N_CELDAS; id++) {
        int f = id / 5, c = id % 5;
        for (int k = 0; k < 4; k++) {
            int nf = f + df[k], nc = c + dc[k];
            if (nf >= 0 && nf < 5 && nc >= 0 && nc < 5)
                agregar_vecino(id, nf * 5 + nc);
        }
    }
}

// El trono es un triangulo: I(25)-T(26)-D(27), con la mediana T-2 y las
// diagonales I-0, I-1, D-3, D-4 (I y D caen sobre el hueco entre columnas).
static void construir_trono(void) {
    agregar_arista(26, 25);   // T-I
    agregar_arista(26, 27);   // T-D
    agregar_arista(25, 27);   // I-D
    agregar_arista(26, 2);    // T-2
    agregar_arista(25, 0);    // I-0
    agregar_arista(25, 1);    // I-1
    agregar_arista(27, 3);    // D-3
    agregar_arista(27, 4);    // D-4
}

static void construir_saltos_grilla(void) {
    int df[4] = {-1, 1, 0, 0};
    int dc[4] = { 0, 0,-1, 1};
    for (int id = 0; id < N_CELDAS; id++) {
        int f = id / 5, c = id % 5;
        for (int k = 0; k < 4; k++) {
            int mf = f + df[k],     mc = c + dc[k];       // casilla comida
            int nf = f + 2 * df[k], nc = c + 2 * dc[k];   // casilla de llegada
            if (nf >= 0 && nf < 5 && nc >= 0 && nc < 5)
                agregar_salto(id, mf * 5 + mc, nf * 5 + nc);
        }
    }
}

static void construir_saltos_trono(void) {
    agregar_salto_doble(26, 25, 0);   // T-I-0
    agregar_salto_doble(26, 27, 4);   // T-D-4
    agregar_salto_doble(25, 1, 6);    // I-1-6
    agregar_salto_doble(27, 3, 8);    // D-3-8
    agregar_salto_doble(26, 2, 7);    // T-2-7
}

static void construir_geometria(void) {
    construir_grilla();
    construir_trono();
    construir_saltos_grilla();
    construir_saltos_trono();
    assert(n_saltos == MAX_SALTOS);
}

// pthread_once: varias mesas pueden llamarla a la vez sin pisarse
void tablero_geometria_init(void) {
    pthread_once(&geometria_once, construir_geometria);
}

int nodo_de(int fila, int col) {
    return fila * 5 + col;
}

bool son_vecinos(int a, int b) {
    for (int i = 0; i < n_adj[a]; i++)
        if (adj[a][i] == b)
            return true;
    return false;
}

int vecinos_de(int nodo, int *salida) {
    for (int i = 0; i < n_adj[nodo]; i++)
        salida[i] = adj[nodo][i];
    return n_adj[nodo];
}

int saltos_desde(int nodo, Salto *salida, int max) {
    int n = 0;
    for (int i = 0; i < n_saltos && n < max; i++)
        if (saltos[i].ini == nodo)
            salida[n++] = saltos[i];
    return n;
}

// Los perros van en el borde sin las esquinas (12 justo). Si el puma ocupa una
// de esas casillas se corre al siguiente candidato de la lista.
static const int orden_perros[N_CELDAS] = {
    1, 2, 3, 5, 9, 10, 14, 15, 19, 21, 22, 23,
    6, 7, 8, 11, 12, 13, 16, 17, 18, 0, 4, 20, 24
};

void tablero_iniciar(Tablero *t, int fila, int col) {
    tablero_geometria_init();
    memset(t, 0, sizeof *t);

    int p = nodo_de(fila, col);
    t->casilla[p] = PUMA;
    t->puma = p;

    int puestos = 0;
    for (int i = 0; i < N_CELDAS && puestos < N_PERROS; i++) {
        int n = orden_perros[i];
        if (n == p)
            continue;
        t->casilla[n] = PERRO;
        puestos++;
    }
    assert(puestos == N_PERROS);
}

void tablero_imprimir(const Tablero *t) {
    const char simbolo[] = {'.', 'P', 'o'};   // indexado por enum Ficha
    printf("trono: %c %c %c\n", simbolo[t->casilla[25]], simbolo[t->casilla[26]],
           simbolo[t->casilla[27]]);
    for (int id = 0; id < N_CELDAS; id++) {
        printf("%c ", simbolo[t->casilla[id]]);
        if (id % 5 == 4)
            printf("\n");
    }
    printf("capturas: %d\n", t->capturas);
}

bool es_movimiento_valido(const Tablero *t, int desde, int hasta) {
    return desde >= 0 && desde < N_NODOS        // 1. desde es un nodo real
        && hasta >= 0 && hasta < N_NODOS        // 2. hasta es un nodo real
        && t->casilla[desde] != LIBRE           // 3. en desde HAY una ficha
        && t->casilla[hasta] == LIBRE           // 4. hasta esta LIBRE
        && son_vecinos(desde, hasta);           // 5. estan a un paso
}

// el trio (a, b, c) es una linea recta de salto
static bool es_salto(int a, int b, int c) {
    for (int i = 0; i < n_saltos; i++)
        if (saltos[i].ini == a && saltos[i].com == b && saltos[i].fin == c)
            return true;
    return false;
}

bool es_captura_valida(const Tablero *t, int desde, int sobre, int hasta) {
    return desde >= 0 && desde < N_NODOS
        && sobre >= 0 && sobre < N_NODOS
        && hasta >= 0 && hasta < N_NODOS
        && t->casilla[desde] == PUMA            // solo el puma come
        && t->casilla[sobre] == PERRO           // y come perros
        && t->casilla[hasta] == LIBRE           // cae en casilla libre
        && es_salto(desde, sobre, hasta);       // en linea recta
}

bool puma_sin_movimientos(const Tablero *t) {
    int p = t->puma;
    int vecinos[MAX_VEC];
    int n = vecinos_de(p, vecinos);
    for (int i = 0; i < n; i++)
        if (es_movimiento_valido(t, p, vecinos[i]))
            return false;

    Salto lista[MAX_SALTOS];
    int ns = saltos_desde(p, lista, MAX_SALTOS);
    for (int i = 0; i < ns; i++)
        if (es_captura_valida(t, p, lista[i].com, lista[i].fin))
            return false;

    return true;
}

bool tablero_mover(Tablero *t, int desde, int hasta) {
    if (!es_movimiento_valido(t, desde, hasta))
        return false;
    enum Ficha f = t->casilla[desde];
    t->casilla[desde] = LIBRE;
    t->casilla[hasta] = f;
    if (f == PUMA)
        t->puma = hasta;
    return true;
}

bool tablero_capturar(Tablero *t, int desde, int sobre, int hasta) {
    if (!es_captura_valida(t, desde, sobre, hasta))
        return false;
    t->casilla[desde] = LIBRE;
    t->casilla[sobre] = LIBRE;
    t->casilla[hasta] = PUMA;
    t->puma = hasta;
    t->capturas++;
    return true;
}

int tablero_contar(const Tablero *t, enum Ficha ficha) {
    int n = 0;
    for (int i = 0; i < N_NODOS; i++)
        if (t->casilla[i] == ficha)
            n++;
    return n;
}
