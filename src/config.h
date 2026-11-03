#ifndef CONFIG_H
#define CONFIG_H

#include "mesa.h"

// Valores por defecto del enunciado: seed 42, 50-300 ms, puma en (2,2).
void config_por_defecto(Config *cfg);

// Lee la PRIMERA linea "CONFIG SEED ... PUMA_INIT ..." del archivo y, si
// estan, las lineas sueltas DESCONEXION_PCT n y TIMEOUT_MS n.
// Devuelve 0 si encontro configuracion valida, -1 si no.
int cargar_config(const char *ruta, Config *cfg);

#endif
