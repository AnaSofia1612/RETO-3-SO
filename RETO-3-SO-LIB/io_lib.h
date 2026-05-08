/**
 * @file io_lib.h
 * @brief Módulo de I/O usando librería estándar de C (stdio.h).
 *
 * En lugar de syscalls directas (open/write/mmap), usa:
 *   fopen()  → abre el archivo
 *   fwrite() → escribe con buffer interno de la libc
 *   fread()  → lee con buffer interno de la libc
 *   fclose() → cierra y vacía el buffer
 *
 * La libc agrupa las escrituras internamente (buffer de 8KB por defecto),
 * reduciendo la cantidad de syscalls reales al kernel vs escribir directo.
 */

#ifndef IO_LIB_H
#define IO_LIB_H

/* Guarda datos binarios usando fwrite() con buffer de libc */
void guardar_archivo_lib(const char* filename, unsigned char* data, int size);

/* Lee archivo binario usando fread() */
unsigned char* leer_archivo_lib(const char* filename, int* size_out);

#endif 
