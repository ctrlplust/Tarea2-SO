#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned seed;
    int delay_min;
    int delay_max;
    int puma_fila;
    int puma_col;
} Config;

void config_por_defecto(Config *cfg) {
    if (!cfg) return;
    cfg->seed = 42;
    cfg->delay_min = 50;
    cfg->delay_max = 300;
    cfg->puma_fila = 2;
    cfg->puma_col = 2;
}

int cargar_config(const char *ruta, Config *cfg) {
    if (!ruta || !cfg) return -1;

    FILE *f = fopen(ruta, "r");
    if (!f) {
        perror("Error al abrir el archivo de configuracion");
        return -1;
    }

    char linea[256];
    int leido = 0;

    while (fgets(linea, sizeof(linea), f)) {
        unsigned seed;
        int dmin, dmax, pf, pc;
        if (sscanf(linea, "CONFIG SEED %u DELAY_MIN %d DELAY_MAX %d PUMA_INIT %d %d",
                   &seed, &dmin, &dmax, &pf, &pc) == 5) {

// Validaciones de integridad
            if (dmin < 0 || dmax < 0 || dmin > dmax) {
                fprintf(stderr, "Advertencia: Delays invalidos (min: %d, max: %d). Usando defaults.\n", dmin, dmax);
                fclose(f);
                return -1;
            }

            if (pf < 0 || pf > 4 || pc < 0 || pc > 4) {
                fprintf(stderr, "Advertencia: Posicion de puma fuera de rango (%d, %d). Usando defaults.\n", pf, pc);
                fclose(f);
                return -1;
            }
            cfg->seed = seed;
            cfg->delay_min = dmin;
            cfg->delay_max = dmax;
            cfg->puma_fila = pf;
            cfg->puma_col = pc;
            leido = 1;
            break;
        }
    }

    fclose(f);
    return leido ? 0 : -1;
}

int main(int argc, char *argv[]) {
    Config cfg;
    config_por_defecto(&cfg);

    if (argc > 1) {
        if (cargar_config(argv[1], &cfg) == 0) {
            printf("Configuracion cargada exitosamente desde '%s':\n", argv[1]);
        } else {
            printf("No se pudo cargar '%s'. Usando valores por defecto.\n", argv[1]);
        }
    } else {
        printf("Sin archivo especificado. Usando valores por defecto.\n");
    }

    printf("  Seed: %u\n", cfg.seed);
    printf("  Delay Min: %d ms\n", cfg.delay_min);
    printf("  Delay Max: %d ms\n", cfg.delay_max);
    printf("  Puma Init: (%d, %d)\n", cfg.puma_fila, cfg.puma_col);

    return 0;
}
