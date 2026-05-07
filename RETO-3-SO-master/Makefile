# Makefile — Reto 03 SO
# Uso: make        -> compila
#      make run    -> compila y ejecuta
#      make strace -> profiling con strace
#      make time   -> mide tiempo de ejecución
#      make clean  -> elimina binarios y archivos generados

CC      = gcc
CFLAGS  = -Wall -Wextra -g
TARGET  = editor
SRCS    = main.c io.c editorTexto.c compresion.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

run: all
	time strace -c ./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf archivos/

.PHONY: all run strace time clean
