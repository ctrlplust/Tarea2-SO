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

int main(void) {
    construir_grilla();
    construir_trono();
    for (int n = 0; n < N_NODOS; n++) {
        printf("%2d:", n);
        for (int i = 0; i < n_adj[n]; i++)
            printf(" %d", adj[n][i]);
        printf("\n");
    }
    return 0;
}
