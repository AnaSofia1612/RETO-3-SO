/**
 * @file editorTexto.h
 * @brief Modulo prinicipal de edicion de texto,
 * tiene la logica del buffer y separacion de bloques de texto en 4KB.
 */

#ifndef EDITORTEXTO_H
#define EDITORTEXTO_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// funcion principal de edicion de texto,
// lee lineas del usuario y las almacena en un bloque de texto dinamico
char* editarTexto(); 

// separa los bloques de texto en bloques de 4KB
// y devuelve un array de punteros a cada bloque.
char** preparar_bloques(const char* texto, int* num_bloques);

#endif // EDITORTEXTO_H
