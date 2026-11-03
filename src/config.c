// Parser de config.txt. Acepta el formato de tests/test_cases.txt para la
// primera prueba y valores extra propios (DESCONEXION_PCT, TIMEOUT_MS).

#include "config.h"

#include <stdio.h>

void config_por_defecto(Config *cfg) {
    cfg->seed = 42;
    cfg->delay_min = 50;
    cfg->delay_max = 300;
    cfg->puma_fila = 2;
    cfg->puma_col = 2;
    cfg->desconexion_pct = 0;
    cfg->timeout_ms = 0;      // 0 = el servidor lo calcula segun los delays
}

int cargar_config(const char *ruta, Config *cfg) {
    FILE *f = fopen(ruta, "r");
    if (!f)
        return -1;

    char linea[256];
    int leido = 0;

    while (fgets(linea, sizeof linea, f)) {
        unsigned seed;
        int dmin, dmax, pf, pc, valor;

        if (!leido &&
            sscanf(linea, "CONFIG SEED %u DELAY_MIN %d DELAY_MAX %d PUMA_INIT %d %d",
                   &seed, &dmin, &dmax, &pf, &pc) == 5) {
            if (dmin < 0 || dmax < dmin || pf < 0 || pf > 4 || pc < 0 || pc > 4) {
                fclose(f);
                return -1;      // configuracion invalida: se usan los defaults
            }
            cfg->seed = seed;
            cfg->delay_min = dmin;
            cfg->delay_max = dmax;
            cfg->puma_fila = pf;
            cfg->puma_col = pc;
            leido = 1;
        } else if (sscanf(linea, "DESCONEXION_PCT %d", &valor) == 1 &&
                   valor >= 0 && valor <= 100) {
            cfg->desconexion_pct = valor;
        } else if (sscanf(linea, "TIMEOUT_MS %d", &valor) == 1 && valor >= 0) {
            cfg->timeout_ms = valor;
        }
    }

    fclose(f);
    return leido ? 0 : -1;
}
