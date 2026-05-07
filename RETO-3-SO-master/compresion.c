/**
 * @file compresion.c
 * @brief Implementación del módulo de compresión/descompresión RLE.
 *
 * Algoritmo RLE (Run-Length Encoding):
 *   - Codifica secuencias de bytes repetidos como [cantidad][carácter].
 *   - Ejemplo: "AAABBC" → {3,'A', 2,'B', 1,'C'} (6 bytes → 6 bytes en este
 *     caso, pero "AAAAAAAABC" → {10,'A',1,'B',1,'C'} = 10 bytes → 6 bytes).
 *   - La cantidad se satura en 255 (máximo de un unsigned char) para no
 *     romper el formato de 2 bytes por run.
 *
 * Formato binario en disco:
 *   [FileHeader 16B][BlockHeader 8B][RLE data...][BlockHeader 8B][RLE...]...
 *
 * Garantías de alineación:
 *   __attribute__((packed)) en las structs elimina el padding del compilador,
 *   asegurando que sizeof(FileHeader)==16 y sizeof(BlockHeader)==8 siempre,
 *   independientemente de la arquitectura. Esto es crítico para que la lectura
 *   con mmap() en io.c interprete los bytes correctamente.
 */

#include "compresion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Helpers privados ────────────────────────────────────────────────────── */

/**
 * @brief Aplica RLE a un único bloque de texto.
 *
 * Recorre el bloque byte a byte contando runs consecutivos.
 * El buffer de salida tiene tamaño máximo 2 × BLOCK_SIZE (peor caso:
 * ningún byte se repite → 1 byte de count + 1 byte de char por cada byte).
 *
 * @param bloque      Puntero al bloque de texto (hasta 4096 bytes).
 * @param len         Longitud efectiva del bloque (sin bytes nulos de relleno).
 * @param out_size    [salida] Bytes escritos en el buffer retornado.
 * @return            Buffer con los datos RLE. El llamador libera con free().
 */
static unsigned char* rle_comprimir_bloque(const char* bloque,
                                            int len,
                                            int* out_size) {
    /* Peor caso: sin repeticiones → 2 bytes por byte de entrada */
    unsigned char* out = malloc(2 * len + 2);
    if (!out) {
        perror("[COMP] malloc rle_comprimir_bloque");
        *out_size = 0;
        return NULL;
    }

    int i = 0;   /* índice en la entrada */
    int j = 0;   /* índice en la salida  */

    while (i < len) {
        unsigned char ch    = (unsigned char)bloque[i];
        unsigned char count = 1;

        /* Contar cuántas veces se repite el carácter actual */
        while (i + (int)count < len
               && (unsigned char)bloque[i + count] == ch
               && count < 255) {
            count++;
        }

        out[j++] = count;  /* primero la cantidad */
        out[j++] = ch;     /* luego el carácter   */
        i += count;
    }

    *out_size = j;
    return out;
}

/**
 * @brief Descomprime un buffer RLE y devuelve el texto recuperado.
 *
 * @param data     Buffer RLE (pares [count][char]).
 * @param size     Bytes en el buffer.
 * @param out_len  [salida] Longitud del texto resultante.
 * @return         String descomprimido. El llamador libera con free().
 */
static char* rle_descomprimir_bloque(const unsigned char* data,
                                      int size,
                                      int* out_len) {
    /* Cota superior: cada par produce hasta 255 caracteres */
    int max_out = (size / 2) * 255 + 1;
    char* out = malloc(max_out);
    if (!out) {
        perror("[COMP] malloc rle_descomprimir_bloque");
        *out_len = 0;
        return NULL;
    }

    int i = 0, j = 0;
    while (i + 1 < size) {
        unsigned char count = data[i];
        unsigned char ch    = data[i + 1];
        for (int k = 0; k < count && j < max_out - 1; k++) {
            out[j++] = (char)ch;
        }
        i += 2;
    }
    out[j] = '\0';
    *out_len = j;
    return out;
}

/* ─── Longitud efectiva de un bloque (sin bytes nulos de relleno) ─────────── */
static int longitud_efectiva(const char* bloque) {
    /* Los bloques fueron inicializados con memset(0) en preparar_bloques(),
       así que strlen() es seguro para hallar el contenido real. */
    return (int)strlen(bloque);
}

/* ─── API pública ─────────────────────────────────────────────────────────── */

unsigned char* comprimir_bloques(char** bloques, int num_bloques, int* size_out) {
    if (!bloques || num_bloques <= 0 || !size_out) return NULL;

    /*
     * PASO 1 — Comprimir cada bloque individualmente y guardar resultados
     *           en arrays temporales para poder calcular el tamaño total
     *           antes de hacer una sola reserva grande.
     */
    unsigned char** rle_data  = malloc(num_bloques * sizeof(unsigned char*));
    int*            rle_sizes = malloc(num_bloques * sizeof(int));

    if (!rle_data || !rle_sizes) {
        perror("[COMP] malloc arrays temporales");
        free(rle_data);
        free(rle_sizes);
        return NULL;
    }

    int size_datos_comprimidos = 0;

    for (int i = 0; i < num_bloques; i++) {
        int len = longitud_efectiva(bloques[i]);
        rle_data[i] = rle_comprimir_bloque(bloques[i], len, &rle_sizes[i]);

        if (!rle_data[i]) {
            /* Limpieza parcial ante error */
            for (int k = 0; k < i; k++) free(rle_data[k]);
            free(rle_data);
            free(rle_sizes);
            *size_out = 0;
            return NULL;
        }

        size_datos_comprimidos += rle_sizes[i];

        printf("[COMP] Bloque %d: %d bytes → %d bytes comprimidos (RLE)\n",
               i, len, rle_sizes[i]);
    }

    /*
     * PASO 2 — Calcular tamaño total del payload y reservar el buffer final.
     *
     *   payload = FileHeader
     *           + num_bloques × (BlockHeader + datos_comprimidos_i)
     */
    int total = (int)sizeof(FileHeader)
              + num_bloques * (int)sizeof(BlockHeader)
              + size_datos_comprimidos;

    unsigned char* payload = malloc(total);
    if (!payload) {
        perror("[COMP] malloc payload final");
        for (int i = 0; i < num_bloques; i++) free(rle_data[i]);
        free(rle_data);
        free(rle_sizes);
        *size_out = 0;
        return NULL;
    }

    /*
     * PASO 3 — Escribir FileHeader al inicio del payload.
     *           Usamos memcpy sobre la struct packed para evitar
     *           problemas de alineación en arquitecturas estrictas.
     */
    FileHeader fh;
    fh.magic        = MAGIC_NUMBER;
    fh.version      = VERSION;
    fh.num_bloques  = num_bloques;
    fh.size_total   = size_datos_comprimidos;
    memcpy(payload, &fh, sizeof(FileHeader));

    int offset = sizeof(FileHeader);

    /*
     * PASO 4 — Escribir BlockHeader + datos RLE de cada bloque.
     */
    for (int i = 0; i < num_bloques; i++) {
        BlockHeader bh;
        bh.bloque_id   = i;
        bh.size_bloque = rle_sizes[i];
        memcpy(payload + offset, &bh, sizeof(BlockHeader));
        offset += sizeof(BlockHeader);

        memcpy(payload + offset, rle_data[i], rle_sizes[i]);
        offset += rle_sizes[i];

        free(rle_data[i]);  /* ya no necesitamos el buffer temporal */
    }

    free(rle_data);
    free(rle_sizes);

    *size_out = total;
    printf("[COMP] Payload final: %d bytes "
           "(FileHeader=%zu + %d bloques comprimidos)\n",
           total, sizeof(FileHeader), num_bloques);
    return payload;
}

/* ──────────────────────────────────────────────────────────────────────────── */

char* descomprimir_bloques(unsigned char* data, int size, int* out_len) {
    if (!data || size < (int)sizeof(FileHeader) || !out_len) return NULL;

    /*
     * PASO 1 — Leer y validar FileHeader.
     */
    FileHeader fh;
    memcpy(&fh, data, sizeof(FileHeader));

    if (fh.magic != MAGIC_NUMBER) {
        fprintf(stderr,
                "[COMP] Error: magic inválido (0x%X != 0x%X). "
                "¿El archivo fue generado con este formato?\n",
                fh.magic, MAGIC_NUMBER);
        return NULL;
    }

    if (fh.version != VERSION) {
        fprintf(stderr, "[COMP] Advertencia: versión %d, se esperaba %d\n",
                fh.version, VERSION);
    }

    printf("[COMP] Metadata leída — bloques: %d, bytes comprimidos: %d\n",
           fh.num_bloques, fh.size_total);

    /*
     * PASO 2 — Reservar buffer de salida para el texto reconstruido.
     *           Cota generosa: cada byte comprimido puede expandirse ×255.
     */
    int max_texto = fh.size_total * 255 + fh.num_bloques + 1;
    char* texto   = malloc(max_texto);
    if (!texto) { perror("[COMP] malloc texto"); return NULL; }

    int offset     = sizeof(FileHeader);
    int pos_texto  = 0;

    /*
     * PASO 3 — Recorrer bloques: leer BlockHeader → descomprimir RLE.
     */
    for (int i = 0; i < fh.num_bloques; i++) {
        if (offset + (int)sizeof(BlockHeader) > size) {
            fprintf(stderr, "[COMP] Error: payload truncado en bloque %d\n", i);
            free(texto);
            return NULL;
        }

        BlockHeader bh;
        memcpy(&bh, data + offset, sizeof(BlockHeader));
        offset += sizeof(BlockHeader);

        if (bh.bloque_id != i) {
            fprintf(stderr,
                    "[COMP] Advertencia: se esperaba bloque %d, se leyó %d\n",
                    i, bh.bloque_id);
        }

        if (offset + bh.size_bloque > size) {
            fprintf(stderr, "[COMP] Error: datos del bloque %d fuera de rango\n", i);
            free(texto);
            return NULL;
        }

        int bloque_len = 0;
        char* bloque_texto = rle_descomprimir_bloque(data + offset,
                                                      bh.size_bloque,
                                                      &bloque_len);
        offset += bh.size_bloque;

        if (!bloque_texto) { free(texto); return NULL; }

        /* Concatenar al buffer de salida */
        if (pos_texto + bloque_len < max_texto - 1) {
            memcpy(texto + pos_texto, bloque_texto, bloque_len);
            pos_texto += bloque_len;
        }
        free(bloque_texto);

        printf("[COMP] Bloque %d descomprimido: %d bytes RLE → %d bytes texto\n",
               i, bh.size_bloque, bloque_len);
    }

    texto[pos_texto] = '\0';
    *out_len = pos_texto;

    printf("[COMP] Texto recuperado: %d bytes totales\n", pos_texto);
    return texto;
}

/* ──────────────────────────────────────────────────────────────────────────── */

void imprimir_metadata(unsigned char* data, int size) {
    if (!data || size < (int)sizeof(FileHeader)) {
        printf("[META] Buffer inválido o demasiado pequeño.\n");
        return;
    }

    FileHeader fh;
    memcpy(&fh, data, sizeof(FileHeader));

    printf("\n╔══════════════════════════════════════╗\n");
    printf("║        METADATA DEL ARCHIVO BIN      ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║ Magic    : 0x%08X              ║\n", fh.magic);
    printf("║ Versión  : %d                         ║\n", fh.version);
    printf("║ Bloques  : %d                         ║\n", fh.num_bloques);
    printf("║ Bytes    : %d (comprimidos)           ║\n", fh.size_total);
    printf("╠══════════════════════════════════════╣\n");

    int offset = sizeof(FileHeader);
    for (int i = 0; i < fh.num_bloques && offset + (int)sizeof(BlockHeader) <= size; i++) {
        BlockHeader bh;
        memcpy(&bh, data + offset, sizeof(BlockHeader));
        offset += sizeof(BlockHeader);

        printf("║  Bloque[%d] id=%-3d size=%-5d bytes  ║\n",
               i, bh.bloque_id, bh.size_bloque);
        offset += bh.size_bloque;
    }

    printf("╚══════════════════════════════════════╝\n\n");
}
