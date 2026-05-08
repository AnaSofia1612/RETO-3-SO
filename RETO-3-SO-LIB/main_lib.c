/**
 * @file main.c  (versión librería estándar)
 * @brief Reto 03 SO — Editor de archivos con I/O usando stdio.h.
 *
 * Mismo pipeline que la versión con syscalls, pero usando:
 *   fopen() + fwrite() + fread() + fclose()
 * en lugar de open() + write() + mmap() + close()
 *
 * Los archivos .bin se guardan en ./archivos/
 */

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#include "io_lib.h"
#include "editorTexto.h"
#include "compresion.h"

#define DIR_ARCHIVOS "archivos"

/* ─── Stats de I/O desde /proc/self/io ────────────────────────────────────── */

typedef struct {
    long rchar;
    long wchar;
    long syscr;
    long syscw;
    long read_bytes;
    long write_bytes;
} IOStats;

static void leer_io_stats(IOStats* s) {
    FILE* f = fopen("/proc/self/io", "r");
    if (!f) { memset(s, 0, sizeof(*s)); return; }
    fscanf(f, "rchar: %ld\n",       &s->rchar);
    fscanf(f, "wchar: %ld\n",       &s->wchar);
    fscanf(f, "syscr: %ld\n",       &s->syscr);
    fscanf(f, "syscw: %ld\n",       &s->syscw);
    fscanf(f, "read_bytes: %ld\n",  &s->read_bytes);
    fscanf(f, "write_bytes: %ld\n", &s->write_bytes);
    fclose(f);
}

static void mostrar_stats(const char* nombre, IOStats* antes, IOStats* despues,
                           double ms_escritura, double ms_lectura) {
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║     RESUMEN I/O — '%s'\n", nombre);
    printf("║     Método: fopen/fwrite/fread (librería)    \n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Bytes escritos al disco  : %-8ld          ║\n",
           despues->write_bytes - antes->write_bytes);
    printf("║ Bytes leídos del disco   : %-8ld          ║\n",
           despues->read_bytes  - antes->read_bytes);
    printf("║ Syscalls de escritura    : %-8ld          ║\n",
           despues->syscw - antes->syscw);
    printf("║ Syscalls de lectura      : %-8ld          ║\n",
           despues->syscr - antes->syscr);
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Tiempo escritura (fwrite): %.4f ms         ║\n", ms_escritura);
    printf("║ Tiempo lectura   (fread) : %.4f ms         ║\n", ms_lectura);
    printf("╚══════════════════════════════════════════════╝\n\n");
}

/* ─── Helpers ──────────────────────────────────────────────────────────────── */

static void crear_directorio() {
    struct stat st = {0};
    if (stat(DIR_ARCHIVOS, &st) == -1) {
        mkdir(DIR_ARCHIVOS, 0755);
        printf("[DIR] Carpeta '%s/' creada.\n", DIR_ARCHIVOS);
    }
}

static void construir_ruta(const char* nombre, char* buf, int buf_size) {
    snprintf(buf, buf_size, "%s/%s.bin", DIR_ARCHIVOS, nombre);
}

static int listar_archivos() {
    DIR* d = opendir(DIR_ARCHIVOS);
    if (!d) { printf("  (carpeta vacía o no existe)\n"); return 0; }

    struct dirent* entry;
    int encontrados = 0;
    while ((entry = readdir(d)) != NULL) {
        const char* n = entry->d_name;
        size_t len = strlen(n);
        if (len > 4 && strcmp(n + len - 4, ".bin") == 0) {
            printf("  [%d] %.*s\n", encontrados + 1, (int)(len - 4), n);
            encontrados++;
        }
    }
    closedir(d);
    if (encontrados == 0) printf("  (no hay archivos guardados todavía)\n");
    return encontrados;
}

/* ─── Flujo: crear archivo nuevo  */

static void flujo_nuevo() {
    char nombre[256];
    printf("\nNombre del archivo (sin extensión): ");
    fgets(nombre, sizeof(nombre), stdin);
    nombre[strcspn(nombre, "\n")] = '\0';

    if (strlen(nombre) == 0) { printf("Nombre inválido.\n"); return; }

    char ruta[512];
    construir_ruta(nombre, ruta, sizeof(ruta));

    struct stat st;
    if (stat(ruta, &st) == 0) {
        printf("El archivo '%s' ya existe. ¿Sobreescribir? (s/n): ", nombre);
        char resp[4];
        fgets(resp, sizeof(resp), stdin);
        if (resp[0] != 's' && resp[0] != 'S') {
            printf("Operación cancelada.\n");
            return;
        }
    }

    /* ── FASE 1: captura y segmentación ── */
    printf("\n");
    char* texto_usuario = editarTexto();

    if (!texto_usuario || strlen(texto_usuario) == 0) {
        printf("Error: No se capturó texto.\n");
        if (texto_usuario) free(texto_usuario);
        return;
    }

    int    num_bloques = 0;
    char** bloques_4kb = preparar_bloques(texto_usuario, &num_bloques);
    if (!bloques_4kb) {
        printf("Error al preparar bloques.\n");
        free(texto_usuario);
        return;
    }

    printf("\n[FASE 1] %d bloque(s) de 4096 bytes generados.\n", num_bloques);
    printf("\n--- INSPECCIÓN DE BLOQUES ---\n");
    for (int i = 0; i < num_bloques; i++)
        printf("Bloque [%d] dir=%p | primeros 40 chars: [%.40s...]\n",
               i, (void*)bloques_4kb[i], bloques_4kb[i]);

    /* ── FASE 2: compresión RLE ── */
    printf("\n[FASE 2] Comprimiendo con RLE...\n");
    int            payload_size = 0;
    unsigned char* payload      = comprimir_bloques(bloques_4kb,
                                                     num_bloques,
                                                     &payload_size);
    if (!payload) { printf("Error en la compresión.\n"); goto cleanup; }

    imprimir_metadata(payload, payload_size);

    int bytes_orig = (int)strlen(texto_usuario);
    printf("[FASE 2] Original : %d bytes\n", bytes_orig);
    printf("[FASE 2] Payload  : %d bytes (con headers)\n", payload_size);
    if (bytes_orig > 0)
        printf("[FASE 2] Ratio    : %.2f%%\n\n",
               100.0 * payload_size / bytes_orig);

    /* ── FASE 3: escritura con fwrite() ── */
    IOStats antes, despues;
    leer_io_stats(&antes);

    struct timespec t0, t1, t2, t3;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    printf("[FASE 3] Guardando '%s' con fwrite()...\n", ruta);
    guardar_archivo_lib(ruta, payload, payload_size);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    free(payload);

    /* ── FASE 4: lectura con fread() ── */
    clock_gettime(CLOCK_MONOTONIC, &t2);

    printf("[FASE 4] Leyendo '%s' con fread()...\n", ruta);
    int            leido_size = 0;
    unsigned char* leido      = leer_archivo_lib(ruta, &leido_size);

    clock_gettime(CLOCK_MONOTONIC, &t3);
    leer_io_stats(&despues);

    if (!leido) { printf("Error al leer el archivo.\n"); goto cleanup; }
    printf("[FASE 4] %d bytes leídos.\n\n", leido_size);

    /* ── FASE 5: descompresión y verificación ── */
    printf("[FASE 5] Descomprimiendo y verificando integridad...\n");
    int   out_len    = 0;
    char* recuperado = descomprimir_bloques(leido, leido_size, &out_len);
    free(leido);

    if (recuperado) {
        printf("\n=== TEXTO RECUPERADO ===\n%s\n========================\n\n",
               recuperado);
        if (strcmp(texto_usuario, recuperado) == 0)
            printf("[OK] Verificación de integridad: TEXTO IDÉNTICO AL ORIGINAL\n");
        else
            printf("[WARN] Diferencias detectadas.\n");
        free(recuperado);
    }

    /* ── Tiempos y stats ── */
    double ms_escritura = ((t1.tv_sec - t0.tv_sec) * 1e9 +
                           (t1.tv_nsec - t0.tv_nsec)) / 1e6;
    double ms_lectura   = ((t3.tv_sec - t2.tv_sec) * 1e9 +
                           (t3.tv_nsec - t2.tv_nsec)) / 1e6;

    mostrar_stats(nombre, &antes, &despues, ms_escritura, ms_lectura);

cleanup:
    free(texto_usuario);
    for (int i = 0; i < num_bloques; i++) free(bloques_4kb[i]);
    free(bloques_4kb);
}

/* ─── Flujo: abrir archivo existente */

static void flujo_abrir() {
    printf("\nArchivos disponibles:\n");
    int total = listar_archivos();
    if (total == 0) return;

    char nombre[256];
    printf("\nNombre del archivo a abrir (sin extensión): ");
    fgets(nombre, sizeof(nombre), stdin);
    nombre[strcspn(nombre, "\n")] = '\0';

    if (strlen(nombre) == 0) { printf("Nombre inválido.\n"); return; }

    char ruta[512];
    construir_ruta(nombre, ruta, sizeof(ruta));

    printf("\n[FASE 4] Leyendo '%s' con fread()...\n", ruta);
    int            leido_size = 0;
    unsigned char* leido      = leer_archivo_lib(ruta, &leido_size);

    if (!leido) {
        printf("Error: no se pudo abrir '%s'.\n"
               "       Verifica el nombre con la opción 3.\n", ruta);
        return;
    }

    printf("[FASE 4] %d bytes leídos.\n", leido_size);
    imprimir_metadata(leido, leido_size);

    printf("[FASE 5] Descomprimiendo...\n");
    int   out_len    = 0;
    char* recuperado = descomprimir_bloques(leido, leido_size, &out_len);
    free(leido);

    if (recuperado) {
        printf("\n╔══════════════════════════════════════╗\n");
        printf("║     CONTENIDO DE '%s'\n", nombre);
        printf("╚══════════════════════════════════════╝\n");
        printf("%s\n", recuperado);
        printf("════════════════════════════════════════\n");
        printf("[OK] %d bytes de texto recuperado.\n", out_len);
        free(recuperado);
    } else {
        printf("[ERROR] No se pudo descomprimir el archivo.\n");
    }
}

/* ─── Menú principal  */

int main() {
    crear_directorio();

    while (1) {
        printf("\n╔══════════════════════════════════════╗\n");
        printf("║  Reto 03 SO — Editor (stdio / lib)   ║\n");
        printf("╠══════════════════════════════════════╣\n");
        printf("║  1. Crear / editar nuevo archivo     ║\n");
        printf("║  2. Abrir archivo existente          ║\n");
        printf("║  3. Listar archivos guardados        ║\n");
        printf("║  4. Salir                            ║\n");
        printf("╚══════════════════════════════════════╝\n");
        printf("Opción: ");

        char opcion[8];
        fgets(opcion, sizeof(opcion), stdin);

        switch (opcion[0]) {
            case '1': flujo_nuevo();  break;
            case '2': flujo_abrir();  break;
            case '3':
                printf("\nArchivos en '%s/':\n", DIR_ARCHIVOS);
                listar_archivos();
                break;
            case '4':
                printf("\n[SISTEMA] Hasta luego.\n");
                return EXIT_SUCCESS;
            default:
                printf("Opción inválida.\n");
        }
    }
    return EXIT_SUCCESS;
}
