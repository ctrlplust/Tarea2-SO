#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <time.h>

#include "config.h"
#include "mesa.h"
#include "tablero.h"

// --- ESTRUCTURA DE CONFIGURACIÓN (Paso 6) ---
typedef struct {
    int puerto;
    int max_mesas;
    int timeout_jugador;
} ConfigServidor;

// --- VARIABLES GLOBALES Y DE CONCURRENCIA (Paso 5 y 7) ---
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

// --- PARSER DE CONFIGURACIÓN CORREGIDO (Paso 6) ---
int cargar_configuracion(const char *ruta_archivo, ConfigServidor *config) {
    // 1. Asignar valores por defecto PRIMERO
    config->puerto = 8080;
    config->max_mesas = 2;
    config->timeout_jugador = 30;

    FILE *f = fopen(ruta_archivo, "r");
    if (!f) {
        return -1; // Mantiene los valores por defecto si no existe el archivo
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), f)) {
        char clave[128], valor[128];
        if (sscanf(linea, "%127[^=]=%127s", clave, valor) == 2) {
            if (strcmp(clave, "PUERTO") == 0) config->puerto = atoi(valor);
            else if (strcmp(clave, "MAX_MESAS") == 0) config->max_mesas = atoi(valor);
            else if (strcmp(clave, "TIMEOUT") == 0) config->timeout_jugador = atoi(valor);
        }
    }

    fclose(f);
    return 0;
}

// --- MANEJADOR DE SEÑALES SIGINT (Paso 7) ---
void manejador_sigint(int sig) {
    (void)sig;
    servidor_activo = 0;

    const char msg[] = "\n[SIGINT recibido] Iniciando cierre limpio del servidor...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    pthread_mutex_lock(&mutex_estado);
    pthread_cond_broadcast(&cond_cola);
    pthread_mutex_unlock(&mutex_estado);

    for (int i = 0; i < 1000; i++) {
        sem_post(&sem_mesas);
    }
}

static int servidor_debe_abortar(void) {
    return !servidor_activo;
}
// --- HILO DE MESA DE JUEGO (Paso 5) ---
void *hilo_mesa(void *arg) {
    DatosMesa *datos = (DatosMesa *)arg;

    printf("  [Mesa %d] Partida iniciada: Puma (Jugador %d) vs Perro (Jugador %d)\n",
           datos->id_mesa, datos->id_puma, datos->id_perro);

    // Configuración de la partida (posicion inicial Puma 2,2 grilla central)
    Config cfg = {
        .seed = (unsigned)time(NULL) + datos->id_mesa,
        .delay_min = 10,
        .delay_max = 50,
        .puma_fila = 2,
        .puma_col = 2,
        .desconexion_pct = 0,
        .timeout_ms = 30000
    };

    // Ejecuta la lógica del juego de Tomi
    int resultado = jugar_partida(datos->id_mesa, &cfg, servidor_debe_abortar);

    // Reporte de resultados
    if (resultado == 0) {
        printf("  [Mesa %d] Partida finalizada: ¡Ganó el Puma!\n", datos->id_mesa);
    } else if (resultado == 1) {
        printf("  [Mesa %d] Partida finalizada: ¡Ganaron los Perros!\n", datos->id_mesa);
    } else if (resultado == 2) {
        printf("  [Mesa %d] Partida finalizada: Tiempo/Movimientos agotados.\n", datos->id_mesa);
    } else {
        printf("  [Mesa %d] Partida abortada por cierre del servidor.\n", datos->id_mesa);
    }

    pthread_mutex_lock(&mutex_estado);
    mesas_activas--;
    pthread_mutex_unlock(&mutex_estado);

    sem_post(&sem_mesas);
    free(datos);
    return NULL;
}
// --- HILO JUGADOR / MATCHMAKER (Paso 5) ---
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
        printf("Jugador %d entra a la cola de espera...\n", id);

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

        pthread_create(&hilos_mesas[idx_m], NULL, hilo_mesa, datos);
    }

    return NULL;
}

// --- MAIN DEL SERVIDOR COMPLETO ---
int main(int argc, char *argv[]) {
    const char *archivo_config = "server.conf";
    if (argc > 1) {
        archivo_config = argv[1];
    }

    ConfigServidor config;
    if (cargar_configuracion(archivo_config, &config) != 0) {
        printf("Aviso: No se pudo abrir '%s'. Usando valores por defecto (Puerto: %d, Max Mesas: %d, Timeout: %ds).\n\n",
               archivo_config, config.puerto, config.max_mesas, config.timeout_jugador);
    }

    max_mesas = config.max_mesas;

    printf("=== SERVIDOR KOMIKAN INICIADO ===\n");
    printf("Puerto: %d | Max Mesas: %d | Timeout: %ds\n", 
           config.puerto, config.max_mesas, config.timeout_jugador);
    printf("Presiona Ctrl + C para detener de forma limpia.\n\n");

    // Configurar sigaction (Paso 7)
    struct sigaction sa;
    sa.sa_handler = manejador_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error al configurar sigaction");
        return 1;
    }

    sem_init(&sem_mesas, 0, max_mesas);

    // Simulación de llegada de jugadores
    int total_jugadores = 20;
    pthread_t hilos_jugadores[total_jugadores];
    int num_jugadores_creados = 0;

    for (int i = 0; i < total_jugadores && servidor_activo; i++) {
        int *id = malloc(sizeof(int));
        *id = i + 1;
        if (pthread_create(&hilos_jugadores[i], NULL, hilo_jugador, id) == 0) {
            num_jugadores_creados++;
        } else {
            free(id);
        }
        usleep(40000);
    }

    // Esperar cierre de hilos
    for (int i = 0; i < num_jugadores_creados; i++) {
        pthread_join(hilos_jugadores[i], NULL);
    }

    for (int i = 0; i < num_mesas_creadas; i++) {
        pthread_join(hilos_mesas[i], NULL);
    }

    // Limpieza final de recursos
    sem_destroy(&sem_mesas);
    pthread_mutex_destroy(&mutex_estado);
    pthread_cond_destroy(&cond_cola);

    printf("\n=== Servidor Komikan finalizado exitosamente ===\n");
    return 0;
}
