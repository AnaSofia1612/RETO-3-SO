/**
 * @file compresion.h
 * @brief Módulo de compresión RLE con header y metadata binaria.
 *
 * Define el formato del archivo binario comprimido:
 *
 *  ┌──────────────────────────────────────────┐
 *  │  FileHeader  (16 bytes, packed)           │
 *  │    magic      : 0x524C4543  ("RLEC")      │
 *  │    version    : 1                         │
 *  │    num_bloques: cantidad de bloques       │
 *  │    size_total : bytes comprimidos totales │
 *  ├──────────────────────────────────────────┤
 *  │  Por cada bloque:                        │
 *  │    BlockHeader (8 bytes, packed)          │
 *  │      bloque_id   : índice del bloque      │
 *  │      size_bloque : bytes comprimidos      │
 *  │    Datos comprimidos (RLE: [cnt][char])  │
 *  └──────────────────────────────────────────┘
 *
 * Pipeline:
 *   char** bloques  →  comprimir_bloques()  →  unsigned char* payload
 *   unsigned char* payload  →  guardar_archivo()  →  file.bin
 *   file.bin  →  leer_archivo()  →  descomprimir_bloques()  →  char*
 */

#ifndef COMPRESION_H
#define COMPRESION_H

#include <stddef.h>

/* ─── Magic number del formato ────────────────────────────────────────────── */
#define MAGIC_NUMBER  0x434C4552   /* "RELC" en little-endian → "RLEC" */
#define VERSION       1
#define BLOCK_SIZE    4096

/* ─── Estructuras del formato binario ─────────────────────────────────────── */

/**
 * @brief Encabezado global del archivo .bin
 * __attribute__((packed)) garantiza que no hay padding entre campos,
 * dando un tamaño exacto y predecible en disco.
 */
typedef struct __attribute__((packed)) {
    int magic;        /**< Número mágico para validar el formato */
    int version;      /**< Versión del formato (actualmente 1)   */
    int num_bloques;  /**< Cantidad total de bloques comprimidos  */
    int size_total;   /**< Suma de todos los bytes comprimidos    */
} FileHeader;

/**
 * @brief Encabezado por bloque — precede los datos RLE de cada bloque.
 */
typedef struct __attribute__((packed)) {
    int bloque_id;    /**< Índice del bloque (0-based)            */
    int size_bloque;  /**< Bytes comprimidos en este bloque       */
} BlockHeader;

/* ─── API pública ─────────────────────────────────────────────────────────── */

/**
 * @brief Comprime un array de bloques de texto con RLE y construye el
 *        payload binario completo (FileHeader + N×(BlockHeader + datos RLE)).
 *
 * @param bloques     Array de punteros a bloques de 4KB (de preparar_bloques).
 * @param num_bloques Cantidad de bloques en el array.
 * @param size_out    [salida] Tamaño en bytes del buffer retornado.
 * @return            Buffer dinámico con todo el payload listo para guardar.
 *                    El llamador debe liberarlo con free().
 *                    NULL si hay error de memoria.
 */
unsigned char* comprimir_bloques(char** bloques, int num_bloques, int* size_out);

/**
 * @brief Descomprime el payload leído de disco y recupera el texto original.
 *
 * Valida el magic number y reconstruye el texto concatenando todos los
 * bloques descomprimidos en orden.
 *
 * @param data      Buffer con el payload binario (FileHeader + bloques).
 * @param size      Tamaño total del buffer en bytes.
 * @param out_len   [salida] Longitud del texto recuperado (sin '\0').
 * @return          String con el texto recuperado (liberar con free()).
 *                  NULL si el formato es inválido o hay error de memoria.
 */
char* descomprimir_bloques(unsigned char* data, int size, int* out_len);

/**
 * @brief Imprime un resumen legible de la metadata del FileHeader.
 *        Útil para depuración y para la demostración empírica con strace/perf.
 *
 * @param data   Buffer con el payload binario.
 * @param size   Tamaño del buffer.
 */
void imprimir_metadata(unsigned char* data, int size);

#endif /* COMPRESION_H */
