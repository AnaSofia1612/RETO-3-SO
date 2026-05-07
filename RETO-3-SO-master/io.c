/**
 * @file io.c
 * @brief Implementación del módulo de I/O optimizado.
 *
 * Escritura: open() + write() en bloques de 4KB (tamaño de página del SO)
 * para minimizar context switches al kernel.
 * Lectura: mmap() mapea el archivo directo en memoria del proceso,
 * evitando copias extra entre kernel space y user space.
 */

#include "io.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 4096  /* 4KB — tamaño de página del SO */

void guardar_archivo(const char* filename, unsigned char* data, int size) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("[IO] Error abriendo archivo");
        return;
    }

    int escrito = 0;
    while (escrito < size) {
        /* Bloques de 4KB para reducir llamadas a write() */
        int chunk = (size - escrito) < BLOCK_SIZE
                    ? (size - escrito) : BLOCK_SIZE;
        int r = write(fd, data + escrito, chunk);
        if (r < 0) { perror("[IO] Error escribiendo"); close(fd); return; }
        escrito += r;
    }

    close(fd);
    printf("[IO] '%s' guardado: %d bytes en bloques de %d\n",
           filename, size, BLOCK_SIZE);
}

unsigned char* leer_archivo(const char* filename, int* size_out) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) { perror("[IO] Error abriendo archivo"); return NULL; }

    struct stat st;
    fstat(fd, &st);
    int size = st.st_size;

    /* mmap mapea el archivo directo en memoria sin copias extra */
    unsigned char* mapeado = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapeado == MAP_FAILED) {
        perror("[IO] Error en mmap"); close(fd); return NULL;
    }

    /* Copiamos a heap para retornar al caller (él libera con free) */
    unsigned char* buffer = malloc(size);
    if (!buffer) {
        perror("[IO] Error en malloc");
        munmap(mapeado, size); close(fd); return NULL;
    }

    memcpy(buffer, mapeado, size);
    munmap(mapeado, size);
    close(fd);

    *size_out = size;
    printf("[IO] '%s' leído con mmap: %d bytes\n", filename, size);
    return buffer;
}

char* descomprimir_rle(unsigned char* data, int size, int* out_len) {
    /* Formato RLE: pares [cantidad][caracter] */
    char* output = malloc(size * 10);
    if (!output) { perror("[IO] Error en malloc"); return NULL; }

    int i = 0, j = 0;
    while (i < size - 1) {
        unsigned char count = data[i];
        unsigned char ch    = data[i + 1];
        for (int k = 0; k < count; k++) output[j++] = (char)ch;
        i += 2;
    }
    output[j] = '\0';
    *out_len = j;

    printf("[IO] RLE descomprimido: %d bytes -> %d bytes\n", size, j);
    return output;
}