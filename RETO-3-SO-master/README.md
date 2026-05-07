# Reto 03 — Editor de Archivos con Optimización de Bus I/O

**Materia:** Sistemas Operativos  
**Autoras:**
- Ana Sofia Angarita Barrios
- Nawal Oriana Valoyes Renteria
- Maria Laura Tafur

---

## Descripción

Editor de texto en C nativo para Linux que implementa un pipeline completo de I/O optimizado. Ningún archivo viaja al disco en texto claro — el texto se comprime en User Space antes de invocar las syscalls del kernel, reduciendo la carga y latencia del bus I/O.

Los archivos comprimidos se guardan en la carpeta `archivos/` con extensión `.bin`.

---

## Pipeline de Datos

```
[Texto del usuario]
      ↓
 editarTexto()        → captura línea a línea
      ↓
 preparar_bloques()   → segmenta en bloques de 4KB
      ↓
 comprimir_bloques()  → RLE + FileHeader + BlockHeaders
      ↓
 guardar_archivo()    → write() en bloques de 4KB  →  archivos/<nombre>.bin
      ↓
 leer_archivo()       → mmap() sin copias extra
      ↓
 descomprimir_bloques() → reconstruye texto original
      ↓
 [Texto recuperado]
```

---

## Estructura del Proyecto

```
RETO-3-SO/
├── main.c            # Menú interactivo y pipeline completo
├── editorTexto.c     # Captura de texto y segmentación en bloques 4KB
├── editorTexto.h
├── compresion.c      # Algoritmo RLE, FileHeader, BlockHeader
├── compresion.h
├── io.c              # Escritura con write() y lectura con mmap()
├── io.h
├── Makefile
├── README.md
└── archivos/         # Carpeta generada automáticamente con los .bin
```

---

## Compilar y Ejecutar

```bash
# Compilar
make

# Ejecutar (menú interactivo)
make run

# Profiling de syscalls
make strace

# Medir tiempos User / Sys / Real
make time

# Limpiar binarios y archivos generados
make clean
```

---

## Uso del Menú

Al ejecutar `make run` aparece un menú con cuatro opciones:

```
╔══════════════════════════════════════╗
║   Reto 03 SO — Editor con I/O opt.   ║
╠══════════════════════════════════════╣
║  1. Crear / editar nuevo archivo     ║
║  2. Abrir archivo existente          ║
║  3. Listar archivos guardados        ║
║  4. Salir                            ║
╚══════════════════════════════════════╝
```

**Opción 1 — Crear:** pide un nombre, abre el editor de texto (terminar con `SALIR`), comprime y guarda en `archivos/<nombre>.bin`. Si el archivo ya existe, pregunta si sobreescribir.

**Opción 2 — Abrir:** lista los archivos disponibles, pide el nombre y muestra el contenido descomprimido en pantalla.

**Opción 3 — Listar:** muestra todos los `.bin` guardados sin abrirlos.

**Opción 4 — Salir:** termina el programa.

---

## Módulos

### editorTexto.c — Entrada y Bufferización
- Captura el texto del usuario línea a línea
- Segmenta en bloques de 4KB alineados al tamaño de página del SO
- Gestión dinámica de memoria con `malloc` y `realloc`

### compresion.c — Compresión y Formato Binario
- Algoritmo RLE (Run-Length Encoding): codifica runs como `[count][char]`
- `FileHeader` (16 bytes, packed): magic number, versión, número de bloques, tamaño total
- `BlockHeader` (8 bytes, packed): id y tamaño de cada bloque
- `__attribute__((packed))` garantiza tamaños exactos sin padding en cualquier arquitectura
- Valida el magic number `0x434C4552` al leer para detectar archivos corruptos o inválidos

### io.c — I/O Optimizado
- **Escritura:** `open()` + `write()` en bloques de 4KB para minimizar context switches al kernel
- **Lectura:** `mmap()` mapea el archivo directo en el espacio de direcciones del proceso — cero copias extra entre kernel space y user space

---

## Formato Binario del Archivo `.bin`

```
┌─────────────────────────────────────────┐
│  FileHeader (16 bytes, packed)           │
│    magic       : 0x434C4552  ("RELC")   │
│    version     : 1                      │
│    num_bloques : cantidad de bloques    │
│    size_total  : bytes comprimidos      │
├─────────────────────────────────────────┤
│  BlockHeader (8 bytes, packed)           │
│    bloque_id   : índice (0-based)       │
│    size_bloque : bytes RLE del bloque   │
│  Datos RLE: [count][char][count][char]… │
├─────────────────────────────────────────┤
│  BlockHeader + Datos RLE (siguiente)    │
│  …                                      │
└─────────────────────────────────────────┘
```

---

## Resultados de Profiling

### `strace -c ./editor`
| Syscall  | Calls | Descripción |
|----------|-------|-------------|
| `write()`| 2     | Bloques de 4KB — mínimas interrupciones al kernel |
| `mmap()` | 9     | Lectura directa en memoria sin copias |
| Total    | 42    | Syscalls totales del proceso |

### `time ./editor`
| Métrica        | Valor      | Interpretación |
|----------------|------------|----------------|
| `user`         | 0m0.002s   | CPU en user space (compresión RLE, mallocs) |
| `sys`          | 0m0.000s   | CPU en kernel space (syscalls) |
| `elapsed`      | 0m0.003s   | Tiempo real total |
| `page faults`  | 90 minor   | Generados por mmap() — esperado y normal |

> `sys` es casi 0 gracias a la reducción de syscalls por escritura en bloques de 4KB y lectura con `mmap()`.  
> `user` sube ligeramente porque el CPU comprime en user space, pero el ahorro neto en bus I/O compensa.

---

## Verificación de Memoria — Valgrind

```bash
valgrind --leak-check=full ./editor
```

Resultado:

```
HEAP SUMMARY: 12 allocs, 12 frees
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

> Cada `malloc` tiene su `free` correspondiente. Sin memory leaks ni errores de memoria.

---

## Herramientas Utilizadas
- **GCC** — compilador C
- **strace** — análisis de syscalls
- **time** — medición de tiempos User/Sys/Real
- **valgrind** — detección de memory leaks
- **make** — automatización de compilación
