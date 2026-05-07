/**
 * @file main.c
 * @brief Reto 03 SO — Editor de archivos con optimización de Bus I/O.
 *
 * Pipeline completo:
 *   [Texto del usuario]
 *       → editarTexto()          (Persona 1)
 *       → preparar_bloques()     (Persona 1)
 *       → comprimir_bloques()    (Persona 2)
 *       → guardar_archivo()      (Persona 3)  →  archivos/<nombre>.bin
 *       → leer_archivo()         (Persona 3)
 *       → descomprimir_bloques() (Persona 2)
 *       → texto recuperado
 *
 * Para medir rendimiento:
 *   make strace   → strace -c ./editor
 *   make time     → time ./editor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#include "io.h"
#include "editorTexto.h"
#include "compresion.h"

#define DIR_ARCHIVOS "archivos"

/* Helpers  */

/** Crea la carpeta ./archivos/ si no existe. */
static void crear_directorio() {
    struct stat st = {0};
    if (stat(DIR_ARCHIVOS, &st) == -1) {
        mkdir(DIR_ARCHIVOS, 0755);
        printf("[DIR] Carpeta '%s/' creada.\n", DIR_ARCHIVOS);
    }
}

/** Construye la ruta completa: "archivos/<nombre>.bin" */
static void construir_ruta(const char* nombre, char* buf, int buf_size) {
    snprintf(buf, buf_size, "%s/%s.bin", DIR_ARCHIVOS, nombre);
}

/** Lista los .bin dentro de ./archivos/ */
static int listar_archivos() {
    DIR* d = opendir(DIR_ARCHIVOS);
    if (!d) {
        printf("  (carpeta '%s/' vacía o no existe)\n", DIR_ARCHIVOS);
        return 0;
    }

    struct dirent* entry;
    int encontrados = 0;
    while ((entry = readdir(d)) != NULL) {
        const char* nombre = entry->d_name;
        size_t len = strlen(nombre);
        if (len > 4 && strcmp(nombre + len - 4, ".bin") == 0) {
            printf("  [%d] %.*s\n", encontrados + 1, (int)(len - 4), nombre);
            encontrados++;
        }
    }
    closedir(d);

    if (encontrados == 0)
        printf("  (no hay archivos guardados todavía)\n");

    return encontrados;
}

/*Flujo: crear archivo nuevo*/

static void flujo_nuevo() {
    char nombre[256];
    printf("\nNombre del archivo (sin extensión): ");
    fgets(nombre, sizeof(nombre), stdin);
    nombre[strcspn(nombre, "\n")] = '\0';

    if (strlen(nombre) == 0) {
        printf("Nombre inválido.\n");
        return;
    }

    char ruta[512];
    construir_ruta(nombre, ruta, sizeof(ruta));

    /* Advertir si el archivo ya existe */
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

    if (texto_usuario == NULL || strlen(texto_usuario) == 0) {
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
    for (int i = 0; i < num_bloques; i++) {
        printf("Bloque [%d] dir=%p | primeros 40 chars: [%.40s...]\n",
               i, (void*)bloques_4kb[i], bloques_4kb[i]);
    }

    /* ── FASE 2: compresión RLE ── */
    printf("\n[FASE 2] Comprimiendo con RLE...\n");
    int            payload_size = 0;
    unsigned char* payload      = comprimir_bloques(bloques_4kb,
                                                     num_bloques,
                                                     &payload_size);
    if (!payload) {
        printf("Error en la compresión.\n");
        goto cleanup;
    }

    imprimir_metadata(payload, payload_size);

    int bytes_orig = (int)strlen(texto_usuario);
    printf("[FASE 2] Original : %d bytes\n", bytes_orig);
    printf("[FASE 2] Payload  : %d bytes (con headers)\n", payload_size);
    if (bytes_orig > 0)
        printf("[FASE 2] Ratio    : %.2f%%\n\n",
               100.0 * payload_size / bytes_orig);

    /* ── FASE 3: escritura a disco ── */
    printf("[FASE 3] Guardando en '%s'...\n", ruta);
    guardar_archivo(ruta, payload, payload_size);
    printf("[FASE 3] ¡Archivo '%s' guardado exitosamente!\n", nombre);

    free(payload);

cleanup:
    free(texto_usuario);
    for (int i = 0; i < num_bloques; i++) free(bloques_4kb[i]);
    free(bloques_4kb);
}

/*  Flujo: abrir archivo existente  */

static void flujo_abrir() {
    printf("\nArchivos disponibles:\n");
    int total = listar_archivos();

    if (total == 0) return;

    char nombre[256];
    printf("\nNombre del archivo a abrir (sin extensión): ");
    fgets(nombre, sizeof(nombre), stdin);
    nombre[strcspn(nombre, "\n")] = '\0';

    if (strlen(nombre) == 0) {
        printf("Nombre inválido.\n");
        return;
    }

    char ruta[512];
    construir_ruta(nombre, ruta, sizeof(ruta));

    /* ── FASE 4: lectura con mmap() ── */
    printf("\n[FASE 4] Leyendo '%s' con mmap()...\n", ruta);
    int            leido_size = 0;
    unsigned char* leido      = leer_archivo(ruta, &leido_size);

    if (!leido) {
        printf("Error: no se pudo abrir '%s'.\n"
               "       Verifica que el nombre sea correcto (opción 3).\n", ruta);
        return;
    }

    printf("[FASE 4] %d bytes leídos.\n", leido_size);
    imprimir_metadata(leido, leido_size);

    /* ── FASE 5: descompresión ── */
    printf("[FASE 5] Descomprimiendo...\n");
    int   out_len    = 0;
    char* recuperado = descomprimir_bloques(leido, leido_size, &out_len);

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

    free(leido);
}

/*  Menú principal  */

int main() {
    crear_directorio();

    while (1) {
        printf("\n╔══════════════════════════════════════╗\n");
        printf("║   Reto 03 SO — Editor con I/O opt.   ║\n");
        printf("╠══════════════════════════════════════╣\n");
        printf("║  1. Crear / editar nuevo archivo     ║\n");
        printf("║  2. Abrir archivo existente          ║\n");
        printf("║  3. Listar archivos guardados        ║\n");
        printf("║  4. Salir y ver resumen              ║\n");
        printf("╚══════════════════════════════════════╝\n");
        printf("Opción: ");

        char opcion[8];
        fgets(opcion, sizeof(opcion), stdin);

        switch (opcion[0]) {
            case '1':
                flujo_nuevo();
                break;
            case '2':
                flujo_abrir();
                break;
            case '3':
                printf("\nArchivos en '%s/':\n", DIR_ARCHIVOS);
                listar_archivos();
                break;
            case '4':
                printf("\n[SISTEMA] Hasta luego.\n");
                return EXIT_SUCCESS;
            default:
                printf("Opción inválida. Intenta de nuevo.\n");
        }
    }

    return EXIT_SUCCESS;
}