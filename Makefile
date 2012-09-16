SOURCES=bitmap.c blend.c
CC=gcc
#CFLAGS=-g -pedantic -Wall -Wextra -std=c99
CFLAGS=-g

all:
	$(CC) $(CFLAGS) $(SOURCES) -o bitmap
