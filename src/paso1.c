#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define N_NODOS 28        // 0-24 grilla, 25=I, 26=T, 27=D
#define MAX_VEC 4         // máximo de líneas que salen de un nodo

int adj[N_NODOS][MAX_VEC];  // adj[n] = lista de vecinos del nodo n
int n_adj[N_NODOS];         // n_adj[n] = cuántos vecinos tiene n

// agrega b como vecino de a (sólo en un sentido)
void agregar_vecino(int a, int b) {
    assert(n_adj[a] < MAX_VEC);
    adj[a][n_adj[a]++] = b;
}

// agrega la línea a-b en ambos sentidos
void agregar_arista(int a, int b) {
    agregar_vecino(a, b);
    agregar_vecino(b, a);
}

void construir_grilla(void) {
    int df[4] = {-1, 1, 0, 0};   // cambio de fila: arriba, abajo, izq, der
    int dc[4] = { 0, 0,-1, 1};   // cambio de columna
    for (int id = 0; id < 25; id++) {
        int f = id / 5;
        int c = id % 5;
        for (int k = 0; k < 4; k++) {
            int nf = f + df[k], nc = c + dc[k];
            if (nf >= 0 && nf < 5 && nc >= 0 && nc < 5)
                agregar_vecino(id, nf * 5 + nc);
        }
    }
}

void construir_trono(void) {
    agregar_arista(26, 25);   // T-I
    agregar_arista(26, 27);   // T-D
    agregar_arista(25, 27);   // I-D
    agregar_arista(26, 2);    // T-2
    agregar_arista(25, 0);    // I-0
    agregar_arista(25, 1);    // I-1
    agregar_arista(27, 3);    // D-3
    agregar_arista(27, 4);    // D-4
}

#define MAX_SALTOS 70     // grilla: 30 líneas x 2 sentidos = 60; trono: 5 x 2 = 10

typedef struct {
    int ini, com, fin;    // sale de ini, salta sobre com (el comido), cae en fin
} Salto;

Salto saltos[MAX_SALTOS];
int n_saltos;

// agrega el salto ini->fin (sólo en un sentido)
void agregar_salto(int ini, int com, int fin) {
    assert(n_saltos < MAX_SALTOS);
    saltos[n_saltos++] = (Salto){ini, com, fin};
}

// agrega el salto en ambos sentidos; el del medio no cambia
void agregar_salto_doble(int a, int b, int c) {
    agregar_salto(a, b, c);
    agregar_salto(c, b, a);
}

// como construir_grilla pero 2 pasos; el for ya genera ambos sentidos
void construir_saltos_grilla(void) {
    int df[4] = {-1, 1, 0, 0};
    int dc[4] = { 0, 0,-1, 1};
    for (int id = 0; id < 25; id++) {
        int f = id / 5;
        int c = id % 5;
        for (int k = 0; k < 4; k++) {
            int mf = f + df[k],     mc = c + dc[k];       // casilla comida
            int nf = f + 2 * df[k], nc = c + 2 * dc[k];   // casilla de llegada
            if (nf >= 0 && nf < 5 && nc >= 0 && nc < 5)
                agregar_salto(id, mf * 5 + mc, nf * 5 + nc);
        }
    }
}

void construir_saltos_trono(void) {
    agregar_salto_doble(26, 25, 0);   // T-I-0
    agregar_salto_doble(26, 27, 4);   // T-D-4
    agregar_salto_doble(25, 1, 6);    // I-1-6
    agregar_salto_doble(27, 3, 8);    // D-3-8
    agregar_salto_doble(26, 2, 7);    // T-2-7
}

enum Ficha { LIBRE, PUMA, PERRO };

typedef struct {
    enum Ficha casilla[N_NODOS];
    int puma;        // nodo donde está el puma
    int capturas;
} Tablero;

bool son_vecinos(int a, int b) {
    for (int i = 0; i < n_adj[a]; i++) {
        if (adj[a][i] == b)
            return true;
    }
    return false;
}

bool es_movimiento_valido(const Tablero *t, int desde, int hasta) {
    return desde >= 0 && desde < N_NODOS        // 1. desde es un nodo real
        && hasta >= 0 && hasta < N_NODOS        // 2. hasta es un nodo real
        && t->casilla[desde] != LIBRE           // 3. en desde HAY una ficha
        && t->casilla[hasta] == LIBRE           // 4. hasta está LIBRE
        && son_vecinos(desde, hasta);           // 5. están a un paso
}

int main(void) {
    construir_grilla();
    construir_trono();
    construir_saltos_grilla();
    construir_saltos_trono();
    assert(n_saltos == MAX_SALTOS);

    for (int n = 0; n < N_NODOS; n++) {
        printf("%2d:", n);
        for (int i = 0; i < n_adj[n]; i++)
            printf(" %d", adj[n][i]);
        printf("\n");
    }
    for (int i = 0; i < n_saltos; i++)
        printf("%2d -(%2d)-> %2d\n", saltos[i].ini, saltos[i].com, saltos[i].fin);

    // son_vecinos
    assert(!son_vecinos(14, 15));   // borde: fin de fila 2 / inicio de fila 3
    assert(!son_vecinos(14, 18));   // una diagonal del 14
    assert( son_vecinos(14, 13));   // un vecino real del 14

    // es_movimiento_valido
    Tablero t = {0};                // todo LIBRE: funciona porque LIBRE = 0 (primero en el enum)
    t.casilla[7] = PERRO;

    assert( es_movimiento_valido(&t, 7, 12));   // válido
    assert(!es_movimiento_valido(&t, 0, 1));    // en desde no hay ficha (falla la condición 3)
    assert(!es_movimiento_valido(&t, 7, 11));   // no es vecino (diagonal)
    assert(!es_movimiento_valido(&t, -1, 0));   // desde fuera de rango
    assert(!es_movimiento_valido(&t, 7, 28));   // hasta fuera de rango

    t.casilla[12] = PUMA;
    t.puma = 12;
    assert(!es_movimiento_valido(&t, 7, 12));   // casilla ocupada

    printf("se aprobo el test, esta weno\n");
    return 0;
}
