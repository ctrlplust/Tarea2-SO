// Servidor del torneo: matchmaker de N jugadores, M mesas concurrentes y
// cierre ordenado con SIGINT. La mesa en si vive en mesa.c (paso 4).
//
//   ./torneo N M [config.txt]

#define _DEFAULT_SOURCE

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "mesa.h"

#define MAX_HILOS_MESA 1000
#define ESPERA_COLA_MS  100
#define LLEGADA_MS       20

static volatile sig_atomic_t servidor_activo = 1;

static pthread_mutex_t mutex_estado = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond_cola    = PTHREAD_COND_INITIALIZER;
static sem_t sem_mesas;

static int esperando_id = -1;      // jugador que espera rival, -1 = nadie
static int llegadas_cerradas;      // ya no van a llegar mas jugadores
static int mesas_activas;
static int max_mesas;
static int contador_mesas;

static pthread_t hilos_mesas[MAX_HILOS_MESA];
static int num_mesas;

static Config cfg;

typedef struct {
    int id_mesa;
    int id_puma;
    int id_perro;
} DatosMesa;

// el juez de cada mesa la consulta en cada vuelta para abortar por SIGINT
static int servidor_cerrando(void) {
    return !servidor_activo;
}

static struct timespec ts_en_ms(long ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return ts;
}

// --- hilo de senales: sigwait, sin locks dentro de un handler ---
static void *hilo_senales(void *arg) {
    sigset_t *set = arg;
    int sig = 0;

    if (sigwait(set, &sig) != 0)
        return NULL;

    servidor_activo = 0;
    const char msg[] = "\n[SIGINT] cerrando el servidor...\n";
    write(STDOUT_FILENO, msg, sizeof msg - 1);

    pthread_mutex_lock(&mutex_estado);
    pthread_cond_broadcast(&cond_cola);   // despierta al que espera rival
    pthread_mutex_unlock(&mutex_estado);

    // las mesas ven servidor_cerrando() en su cond_timedwait
    return NULL;
}

// --- hilo de una mesa: juega y libera el cupo ---
static void *hilo_mesa(void *arg) {
    DatosMesa *d = arg;

    jugar_partida(d->id_mesa, &cfg, servidor_cerrando);

    pthread_mutex_lock(&mutex_estado);
    mesas_activas--;
    pthread_mutex_unlock(&mutex_estado);
    sem_post(&sem_mesas);                 // libera la mesa para el proximo par

    free(d);
    return NULL;
}

// --- hilo de un jugador: llega, se empareja y lanza la mesa ---
static void *hilo_jugador(void *arg) {
    int id = *(int *)arg;
    free(arg);

    pthread_mutex_lock(&mutex_estado);
    if (!servidor_activo) {
        pthread_mutex_unlock(&mutex_estado);
        return NULL;
    }

    if (esperando_id == -1) {
        // no hay nadie: este jugador espera rival en la cola
        esperando_id = id;
        printf("[Cola] jugador %d espera rival\n", id);

        while (esperando_id == id && servidor_activo && !llegadas_cerradas)
            pthread_cond_wait(&cond_cola, &mutex_estado);

        if (esperando_id == id) {
            esperando_id = -1;
            pthread_mutex_unlock(&mutex_estado);
            printf("[Cola] jugador %d se queda sin rival (N impar)\n", id);
            return NULL;
        }
        pthread_mutex_unlock(&mutex_estado);
        return NULL;
    }

    // ya habia uno esperando: se forma la pareja y este hilo crea la mesa
    int id_puma = esperando_id;
    int id_perro = id;
    esperando_id = -1;
    pthread_cond_signal(&cond_cola);
    pthread_mutex_unlock(&mutex_estado);

    // espera PASIVA a que haya una mesa libre (nunca mas de M activas)
    while (servidor_activo) {
        struct timespec ts = ts_en_ms(ESPERA_COLA_MS);
        if (sem_timedwait(&sem_mesas, &ts) == 0)
            break;
        if (errno != EINTR && errno != ETIMEDOUT)
            break;
    }
    if (!servidor_activo)
        return NULL;

    DatosMesa *d = malloc(sizeof *d);
    if (!d) {
        sem_post(&sem_mesas);
        return NULL;
    }

    pthread_mutex_lock(&mutex_estado);
    mesas_activas++;
    assert(mesas_activas <= max_mesas);   // invariante: nunca mas de M mesas
    d->id_mesa = ++contador_mesas;
    pthread_mutex_unlock(&mutex_estado);
    d->id_puma = id_puma;
    d->id_perro = id_perro;

    pthread_t h;
    if (pthread_create(&h, NULL, hilo_mesa, d) == 0) {
        pthread_mutex_lock(&mutex_estado);
        if (num_mesas < MAX_HILOS_MESA)
            hilos_mesas[num_mesas++] = h;
        pthread_mutex_unlock(&mutex_estado);
    } else {
        free(d);
        pthread_mutex_lock(&mutex_estado);
        mesas_activas--;
        pthread_mutex_unlock(&mutex_estado);
        sem_post(&sem_mesas);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s N M [config.txt]\n", argv[0]);
        return 1;
    }

    int n_jugadores = atoi(argv[1]);
    int m_mesas = atoi(argv[2]);
    if (n_jugadores <= 0 || m_mesas <= 0) {
        fprintf(stderr, "N y M deben ser mayores que 0\n");
        return 1;
    }
    if (m_mesas > MAX_HILOS_MESA)
        m_mesas = MAX_HILOS_MESA;
    max_mesas = m_mesas;

    config_por_defecto(&cfg);
    if (argc > 3 && cargar_config(argv[3], &cfg) != 0)
        fprintf(stderr, "Aviso: no se pudo leer '%s', uso valores por defecto\n",
                argv[3]);
    if (cfg.timeout_ms <= 0)
        cfg.timeout_ms = 4 * cfg.delay_max + 500;

    // SIGINT bloqueada en todos los hilos ANTES de crearlos; un hilo la espera
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    pthread_t hilo_sig;
    if (pthread_create(&hilo_sig, NULL, hilo_senales, &set) != 0) {
        perror("pthread_create (senales)");
        return 1;
    }

    sem_init(&sem_mesas, 0, (unsigned)m_mesas);

    printf("=== Torneo Komikan: %d jugadores, %d mesas ===\n", n_jugadores, m_mesas);
    printf("seed=%u delay=%d-%d ms puma=%d,%d desconexion=%d%% timeout=%d ms\n",
           cfg.seed, cfg.delay_min, cfg.delay_max, cfg.puma_fila, cfg.puma_col,
           cfg.desconexion_pct, cfg.timeout_ms);
    fflush(stdout);

    pthread_t *jugadores = malloc(sizeof(pthread_t) * (size_t)n_jugadores);
    if (!jugadores) {
        fprintf(stderr, "sin memoria\n");
        return 1;
    }

    int creados = 0;
    for (int i = 0; i < n_jugadores && servidor_activo; i++) {
        int *id = malloc(sizeof *id);
        if (!id)
            break;
        *id = i + 1;
        if (pthread_create(&jugadores[creados], NULL, hilo_jugador, id) == 0)
            creados++;
        else
            free(id);
        usleep(LLEGADA_MS * 1000);
    }

    // cerrar llegadas: si N era impar, el que quedo solo en la cola se va
    pthread_mutex_lock(&mutex_estado);
    llegadas_cerradas = 1;
    pthread_cond_broadcast(&cond_cola);
    pthread_mutex_unlock(&mutex_estado);

    for (int i = 0; i < creados; i++)
        pthread_join(jugadores[i], NULL);

    pthread_mutex_lock(&mutex_estado);
    int total_mesas = num_mesas;
    pthread_mutex_unlock(&mutex_estado);

    for (int i = 0; i < total_mesas; i++)
        pthread_join(hilos_mesas[i], NULL);

    // el hilo de senales queda en sigwait: se lo cancela para poder joinearlo
    pthread_cancel(hilo_sig);
    pthread_join(hilo_sig, NULL);

    sem_destroy(&sem_mesas);
    pthread_mutex_destroy(&mutex_estado);
    pthread_cond_destroy(&cond_cola);
    free(jugadores);

    printf("=== Servidor cerrado: %d mesas atendidas (activas=%d) ===\n",
           total_mesas, mesas_activas);
    return 0;
}
