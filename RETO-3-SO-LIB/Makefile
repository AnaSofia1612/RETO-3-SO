# Makefile — Reto 03 SO (versión librería estándar)

CC      = gcc
CFLAGS  = -Wall -Wextra -g
TARGET  = editor_lib
SRCS    = main_lib.c io_lib.c editorTexto.c compresion.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

run: clean all
	time strace -c ./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf archivos/

.PHONY: all run strace time clean
