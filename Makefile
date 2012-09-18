SOURCES=bitmap.c blend.c llist.c
CC=gcc
#CFLAGS=-g -pedantic -Wall -Wextra -std=c99
CFLAGS=-g

all:
	$(CC) $(CFLAGS) $(SOURCES) -o bitmap
