CC = gcc

CFLAGS = -Wall

all: gerador parque

gerador: gerador.c
	gcc gerador.c -o gerador -lpthread -lrt

parque: parque.c
	gcc parque.c -o parque -lpthread -lrt

