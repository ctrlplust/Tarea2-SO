#include <assert.h>
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
    return 0;
}
