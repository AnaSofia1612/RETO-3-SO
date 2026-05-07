/**
 * @file editorTexto.h
 * @brief Modulo prinicipal de edicion de texto,
 * tiene la logica del buffer: el cual es recibir el texto digitado por el usuario con un malloc(), 
 que se va incrementando si supera el tamaño dado con un realloc
 y separacion de bloques de texto en 4KB : se realizan dos malloc, para el arrelgo de punteros que tienen la direccion de memoria de 
 los bloques, y la memoria de los bloques en si que contienen el valor que digito el usuario, y estan separados en 4KB los bloques. 
 
 se realiza la debida liberacion de la memoria en el main. 
 */



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "editorTexto.h"


char* editarTexto() {

  char linea[4096];
  char *bloque = NULL;
  int capacidad = 4096; // tamaño de bloque de texto
  int contador = 0; // contador de caracteres escritos en el bloque de texto

    bloque = malloc(capacidad * sizeof(char));
    bloque[0] = '\0'; // para evitar problemas con el strcat pues el busca el caracter
    // nulo en el bloque para unir el otro string. 

    if (bloque == NULL)
    {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }

      printf("---- ¡EDITOR DE TEXTO! ----- \n");
      printf("Puede empezar a escribir su texto \n");
      printf("Para guardar y salir, escriba 'SALIR' para terminar: \n\n");
        
    while(1){
       fgets(linea, 4096, stdin);

        if (strcmp(linea, "SALIR\n") == 0)
        {
            break; // se termina la edicion de texto si el usuario escribe 'SALIR'
        }

        int longitud_linea = strlen(linea);

        if(capacidad < contador + longitud_linea)
        {
            capacidad = capacidad * 2; //  se duplica la capacidad del bloque
            bloque = realloc(bloque, capacidad * sizeof(char));

            if (bloque == NULL)
            {
                perror("Error al asignar memoria");
                exit(EXIT_FAILURE);
            }
        }

        strcat(bloque, linea); // se concatena la nueva linea al bloque de texto
      contador += longitud_linea; // se actualiza el contador de caracteres escritos en el bloque de texto
    }

    // NO SE  UTILIZA FREE PARA LIBERAR LA MEMORIA DEL BLOQUE
    //PORQUE SINO ESTARIAMOS DEVOLVIENDO NADA
    return bloque; // se retorna el bloque de texto editado
}

char** preparar_bloques(const char* texto, int* num_bloques){
  int longitud_total=strlen(texto);
  *num_bloques=(longitud_total+4096-1)/4096; // se calcula el numero de bloques 
  // segun la longitud del texto del usuario, restamos uno para no redondear hacia arriba
  // es decir si tenemos 4096 + 4096 / 4096 nos da 2 bloques
  // lo cual estariamos creando uno vacio e innecesario
  // por eso ponemos -1. 
 if (*num_bloques == 0) *num_bloques = 1; //por si no escriben nada

  char** lista; // se crea un puntero doble que tiene la dir de la primera posicion
  // del arreglo de punteros

  lista = malloc(*num_bloques * sizeof(char*));  // se crea el arreglo de punteros
  // por eso el tipo de dato es char* pq es el punteros tipo char
    if (lista == NULL)
    {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }

    for (int i=0; i<*num_bloques; i++){
        lista[i] = malloc(4096 * sizeof(char)); // recorremos el aggrelo de punteros
        // en cada recorrido se le asigna un puntero vacio del malloc al puntero del 
        // arreglo de punteros,
        if (lista[i] == NULL)
        {
            perror("Error al asignar memoria");
            exit(EXIT_FAILURE);
        }

        memset(lista[i], 0, 4096); // inicializamos el bloque en 0
        // para evitar datos basura debido al malloc, 
        //y evitamos problemas con el memcpy que copia al bloque 4Kb despues

        // aqui es donde copiamos el bloque de texto del usuario al bloque de 4KB,
        // con cada recorrido se copia un bloque diferente empezando
        // dependiendo del indice del bloque, es decir el primer bloque se copia desde el inicio del texto
        // PERO para evitar copiar mas de lo debido debemos calcular por recorrido
        // lo restante del bloque

        int inicio = i * 4096;
        int bytes_restantes = longitud_total - inicio; 
        // calcualos lo que queda del texto

        if (bytes_restantes > 4096){
            bytes_restantes = 4096; // si lo restante del bloque es mayor a 4096 se copia solo 4096
        }else{
            bytes_restantes = bytes_restantes; // si lo restante del bloque es menor a 4096 se copia lo restante
        }

        memcpy(lista[i], &texto[inicio], bytes_restantes); // se copia el bloque de texto al bloque de 4KB


    }
  return lista; 

}






