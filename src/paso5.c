#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <assert.h>

// Primitivas de sincronizacion
sem_t sem_mesas;
pthread_mutex_t mutex_estado = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_cola = PTHREAD_COND_INITIALIZER;

// Variables de estado del Matchmaker
int esperando_id = -1;
int mesas_activas = 0;
int max_mesas = 0;
int contador_mesas = 0;

// Arreglos para join limpio
pthread_t hilos_mesas[1000];
int num_mesas_creadas = 0;

typedef struct {
    int id_mesa;
    int id_puma;
    int id_perro;
} DatosMesa;

// Hilo que simula una mesa de juego (Tomi la reemplazara luego)
void *hilo_mesa_falsa(void *arg) {
    DatosMesa *datos = (DatosMesa *)arg;

    printf("  [Mesa %d] Partida iniciada: Puma (Jugador %d) vs Perro (Jugador %d)\n",
           datos->id_mesa, datos->id_puma, datos->id_perro);

    // Simular duracion de la partida (100 a 300 ms)
    usleep((100 + rand() % 200) * 1000);

    printf("  [Mesa %d] Partida finalizada.\n", datos->id_mesa);

    // Actualizar contador de mesas activas
    pthread_mutex_lock(&mutex_estado);
    mesas_activas--;
    pthread_mutex_unlock(&mutex_estado);

    // Liberar cupo en el semaforo de mesas
    sem_post(&sem_mesas);

    free(datos);
    return NULL;
}

// Hilo de cada jugador
void *hilo_jugador(void *arg) {
    int id = *(int *)arg;
    free(arg);

    pthread_mutex_lock(&mutex_estado);

    if (esperando_id == -1) {
        // No hay nadie esperando: este jugador se queda en la cola
        esperando_id = id;
        printf("Jugador %d entra a la cola y espera rival...\n", id);

        while (esperando_id == id) {
            pthread_cond_wait(&cond_cola, &mutex_estado);
        }
        pthread_mutex_unlock(&mutex_estado);
    } else {
        // Ya habia un jugador esperando: se forma la pareja
        int id_puma = esperando_id;
        int id_perro = id;
        esperando_id = -1;

        printf("Jugador %d llega. ¡Pareja formada! Puma: %d, Perro: %d\n",
               id_perro, id_puma, id_perro);

        pthread_cond_signal(&cond_cola); // Despierta al Puma dormido
        pthread_mutex_unlock(&mutex_estado);

        // Esperar pasivamente a que haya una mesa disponible
        sem_wait(&sem_mesas);

        pthread_mutex_lock(&mutex_estado);
        mesas_activas++;
        assert(mesas_activas <= max_mesas); // Verificacion de invariante
        int id_m = ++contador_mesas;
        int idx_m = num_mesas_creadas++;
        pthread_mutex_unlock(&mutex_estado);

        // Datos para la mesa
        DatosMesa *datos = malloc(sizeof(DatosMesa));
        datos->id_mesa = id_m;
        datos->id_puma = id_puma;
        datos->id_perro = id_perro;

        // Crear hilo de la mesa
        pthread_create(&hilos_mesas[idx_m], NULL, hilo_mesa_falsa, datos);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <num_jugadores> <num_mesas>\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    max_mesas = M;

    srand(42);

    // Inicializar semaforo de mesas con M cupos
    sem_init(&sem_mesas, 0, M);

    pthread_t hilos_jugadores[N];

    printf("=== Servidor Matchmaker Iniciado (%d jugadores, max %d mesas) ===\n\n", N, M);

    // Crear N hilos de jugadores
    for (int i = 0; i < N; i++) {
        int *id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&hilos_jugadores[i], NULL, hilo_jugador, id);
        usleep(10000); // Pequeño delay de llegada entre jugadores
    }

    // Esperar a que todos los hilos jugador terminen su fase de emparejamiento
    for (int i = 0; i < N; i++) {
        pthread_join(hilos_jugadores[i], NULL);
    }

    // Si N era impar, reportar al jugador sobrante
    pthread_mutex_lock(&mutex_estado);
    if (esperando_id != -1) {
        printf("\nJugador %d se quedo sin rival (N impar).\n", esperando_id);
    }
    pthread_mutex_unlock(&mutex_estado);

    // Esperar a que todas las mesas abiertas terminen
    for (int i = 0; i < num_mesas_creadas; i++) {
        pthread_join(hilos_mesas[i], NULL);
    }

    // Limpieza de recursos
    sem_destroy(&sem_mesas);
    pthread_mutex_destroy(&mutex_estado);
    pthread_cond_destroy(&cond_cola);

    printf("\n=== Servidor finalizado limpiamente ===\n");
    return 0;
}
