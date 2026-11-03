// Tablero de Komikan como grafo. Es el paso 1 pasado a modulo.

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

// el del medio no cambia: ida y vuelta comparten la casilla comida
static void agregar_salto_doble(int a, int b, int c) {
    agregar_salto(a, b, c);
    agregar_salto(c, b, a);
}

// la grilla no tiene diagonales: cada casilla se conecta con la de arriba,
// abajo, izquierda y derecha, y solo si existe
static void construir_grilla(void) {
    int df[4] = {-1, 1, 0, 0};
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

// El trono es un triangulo. I(25) y D(27) caen sobre el hueco entre las
// columnas 0-1 y 3-4, por eso cada uno toca dos casillas de la fila 0:
// 25 va con 0 y 1, 27 va con 3 y 4. T(26) esta justo sobre la columna 2
// y baja recto al nodo 2. Los tres se conectan entre si (el triangulo).
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

// salto de dos pasos en la grilla. El for ya genera los dos sentidos, por eso
// aca no se usa agregar_salto_doble
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

// los saltos que pasan por el trono. Son 5 lineas y cada una vale por los dos
// sentidos, por eso van con agregar_salto_doble
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
    assert(n_saltos == MAX_SALTOS);   // 60 de la grilla + 10 del trono
}

// varias mesas pueden llegar aca al mismo tiempo; pthread_once se encarga de
// que construir_geometria corra una sola vez y las demas esperen
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

// El borde sin las esquinas son justo 12 casillas: 1,2,3,5,9,10,14,15,19,21,
// 22,23. El resto de la lista es relleno por si el puma arranca en una de esas
// y hay que correrse a la siguiente.
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
            continue;               // la casilla del puma no lleva perro
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
        && t->casilla[desde] != LIBRE           // 3. en desde hay alguna ficha
        && t->casilla[hasta] == LIBRE           // 4. hasta esta libre
        && son_vecinos(desde, hasta);           // 5. estan a un paso
}

// busca (a,b,c) en la tabla de saltos. Solo mira la geometria, no las fichas
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
        && t->casilla[hasta] == LIBRE           // cae en una libre
        && es_salto(desde, sobre, hasta);       // y el trio es una recta
}

// el puma esta acorralado si no puede caminar a ninguna libre Y tampoco puede
// comer. Con que una de las dos cosas exista, todavia tiene jugada
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
        t->puma = hasta;        // mantener el atajo actualizado
    return true;
}

bool tablero_capturar(Tablero *t, int desde, int sobre, int hasta) {
    if (!es_captura_valida(t, desde, sobre, hasta))
        return false;
    t->casilla[desde] = LIBRE;
    t->casilla[sobre] = LIBRE;  // el perro comido desaparece
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
