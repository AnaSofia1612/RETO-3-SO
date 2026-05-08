/**
 * @file io_lib.c
 * @brief I/O con librería estándar de C
 *
 * Diferencia clave con io.c:
 *
 *  io.c (syscalls directas)        io_lib.c (librería estándar)
 *  ─────────────────────────────   ──────────────────────────────
 *  open()  → fd del kernel         fopen()  → FILE* con buffer en heap
 *  write() → va directo al kernel  fwrite() → acumula en buffer de libc
 *  mmap()  → mapea en memoria      fread()  → copia desde buffer de libc
 *  close() → cierra fd             fclose() → vacía buffer y cierra fd
 *
 * La libc mantiene un buffer interno y solo llama a
 * write() cuando ese buffer se llena o cuando se llama fclose()/fflush().
 * Eso puede reducir el número de syscalls, pero agrega una copia extra
 * en memoria (heap → buffer libc → kernel).
 *
 * Con archivos pequeños (< 8KB) fwrite() puede resultar en UNA sola
 * syscall write() al hacer fclose(), mientras que io.c hace una por
 * cada chunk de 4KB. Con archivos grandes, el comportamiento converge.
 */

#include "io_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE_LIB 4096 

void guardar_archivo_lib(const char* filename, unsigned char* data, int size) {
  
    FILE* f = fopen(filename, "wb");
    if (!f) {
        perror("[IO_LIB] Error abriendo archivo");
        return;
    }

    /* fwrite() copia los datos al buffer interno de la libc.
       La syscall write() real ocurre cuando:
         - el buffer interno se llena (por defecto ~8KB)
         - se llama fflush() o fclose()
       Por eso con archivos pequeños puede generar MENOS syscalls que
       write() directo, pero a costa de una copia extra en memoria. */
    int escrito = 0;
    while (escrito < size) {
        int chunk = (size - escrito) < BLOCK_SIZE_LIB
                    ? (size - escrito) : BLOCK_SIZE_LIB;
        int r = (int)fwrite(data + escrito, 1, chunk, f);
        if (r <= 0) {
            perror("[IO_LIB] Error escribiendo");
            fclose(f);
            return;
        }
        escrito += r;
    }

    /* fclose() vacía el buffer interno y llama a close() */
    fclose(f);
    printf("[IO_LIB] '%s' guardado: %d bytes con fwrite()\n",
           filename, size);
}

unsigned char* leer_archivo_lib(const char* filename, int* size_out) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("[IO_LIB] Error abriendo archivo");
        return NULL;
    }

    /* Obtener tamaño del archivo con fseek/ftell
       (equivalente a fstat() en la versión con syscalls) */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    unsigned char* buffer = malloc((int)size);
    if (!buffer) {
        perror("[IO_LIB] Error en malloc");
        fclose(f);
        return NULL;
    }

    /* fread() lee desde el buffer interno de la libc,
       que a su vez usa read() del kernel internamente */
    size_t leido = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if ((long)leido != size) {
        fprintf(stderr, "[IO_LIB] Warning: se esperaban %ld bytes, se leyeron %zu\n",
                size, leido);
    }

    *size_out = (int)size;
    printf("[IO_LIB] '%s' leído con fread(): %d bytes\n", filename, (int)size);
    return buffer;
}
