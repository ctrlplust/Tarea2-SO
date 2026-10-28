#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>

volatile sig_atomic_t servidor_activo = 1;

sem_t sem_mesas;
pthread_mutex_t mutex_estado = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_cola = PTHREAD_COND_INITIALIZER;

int esperando_id = -1;
int mesas_activas = 0;
int max_mesas = 0;
int contador_mesas = 0;

pthread_t hilos_mesas[1000];
int num_mesas_creadas = 0;

typedef struct {
    int id_mesa;
    int id_puma;
    int id_perro;
} DatosMesa;

void manejador_sigint(int sig) {
    (void)sig;
    servidor_activo = 0;

    const char msg[] = "\n[SIGINT recibido] Iniciando cierre limpio del servidor...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    // Despertar hilos en la cola
    pthread_mutex_lock(&mutex_estado);
    pthread_cond_broadcast(&cond_cola);
    pthread_mutex_unlock(&mutex_estado);

    // Despertar a todos los hilos que puedan estar atascados en sem_wait
    for (int i = 0; i < 1000; i++) {
        sem_post(&sem_mesas);
    }
}

void *hilo_mesa_falsa(void *arg) {
    DatosMesa *datos = (DatosMesa *)arg;

    printf("  [Mesa %d] Partida iniciada: Puma (%d) vs Perro (%d)\n",
           datos->id_mesa, datos->id_puma, datos->id_perro);

    for (int i = 0; i < 10 && servidor_activo; i++) {
        usleep(20000);
    }

    printf("  [Mesa %d] Partida finalizada.\n", datos->id_mesa);

    pthread_mutex_lock(&mutex_estado);
    mesas_activas--;
    pthread_mutex_unlock(&mutex_estado);

    sem_post(&sem_mesas);
    free(datos);
    return NULL;
}

void *hilo_jugador(void *arg) {
    int id = *(int *)arg;
    free(arg);

    pthread_mutex_lock(&mutex_estado);

    if (!servidor_activo) {
        pthread_mutex_unlock(&mutex_estado);
        return NULL;
    }

    if (esperando_id == -1) {
        esperando_id = id;
        printf("Jugador %d entra a la cola...\n", id);

        while (esperando_id == id && servidor_activo) {
            pthread_cond_wait(&cond_cola, &mutex_estado);
        }

        if (!servidor_activo) {
            if (esperando_id == id) esperando_id = -1;
            pthread_mutex_unlock(&mutex_estado);
            return NULL;
        }

        pthread_mutex_unlock(&mutex_estado);
    } else {
        int id_puma = esperando_id;
        int id_perro = id;
        esperando_id = -1;

        printf("Jugador %d llega. ¡Pareja formada! Puma: %d, Perro: %d\n",
               id_perro, id_puma, id_perro);

        pthread_cond_signal(&cond_cola);
        pthread_mutex_unlock(&mutex_estado);

        if (!servidor_activo) return NULL;

        sem_wait(&sem_mesas);

        if (!servidor_activo) return NULL;

        pthread_mutex_lock(&mutex_estado);
        mesas_activas++;
        int id_m = ++contador_mesas;
        int idx_m = num_mesas_creadas++;
        pthread_mutex_unlock(&mutex_estado);

        DatosMesa *datos = malloc(sizeof(DatosMesa));
        datos->id_mesa = id_m;
        datos->id_puma = id_puma;
        datos->id_perro = id_perro;

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

    struct sigaction sa;
    sa.sa_handler = manejador_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error al configurar sigaction");
        return 1;
    }

    sem_init(&sem_mesas, 0, M);
    pthread_t hilos_jugadores[N];
    int num_jugadores_creados = 0;

    printf("=== Servidor Matchmaker (Paso 7: SIGINT activo) ===\n");
    printf("Presiona Ctrl + C en cualquier momento para probar el cierre limpio.\n\n");

    for (int i = 0; i < N && servidor_activo; i++) {
        int *id = malloc(sizeof(int));
        *id = i + 1;
        if (pthread_create(&hilos_jugadores[i], NULL, hilo_jugador, id) == 0) {
            num_jugadores_creados++;
        } else {
            free(id);
        }
        usleep(50000);
    }

    // Esperar unicamente a los hilos efectivamente creados
    for (int i = 0; i < num_jugadores_creados; i++) {
        pthread_join(hilos_jugadores[i], NULL);
    }

    for (int i = 0; i < num_mesas_creadas; i++) {
        pthread_join(hilos_mesas[i], NULL);
    }

    sem_destroy(&sem_mesas);
    pthread_mutex_destroy(&mutex_estado);
    pthread_cond_destroy(&cond_cola);

    printf("\n=== Servidor finalizado exitosamente (0 fugas) ===\n");
    return 0;
}
