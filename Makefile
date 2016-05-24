CC = gcc

CFLAGS = -Wall -lpthread -D_REENTRANT -lrt

all: gerador parque

gerador: gerador.c
	$(CC) gerador.c -o bin/gerador $(CFLAGS)

parque: parque.c
	$(CC) parque.c -o bin/parque $(CFLAGS)

clean:
	rm gerador parque
