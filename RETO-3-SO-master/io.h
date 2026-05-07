/**
 * @file io.h
 * @brief Módulo de I/O optimizado para el editor de archivos.
 * Escritura con open()/write() en bloques de 4KB y lectura con mmap().
 */

#ifndef IO_H
#define IO_H

#include <stddef.h>

/* Guarda datos binarios en disco en bloques de 4KB */
void guardar_archivo(const char* filename, unsigned char* data, int size);

/* Lee archivo binario con mmap(), retorna buffer (liberar con free()) */
unsigned char* leer_archivo(const char* filename, int* size_out);

/* Descomprime datos RLE: [cantidad][caracter] -> texto plano */
char* descomprimir_rle(unsigned char* data, int size, int* out_len);

#endif /* IO_H */