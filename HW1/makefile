CFLAGS=-c -ansi -Wall -pedantic-errors

all: buNeDu clean

buNeDu: buNeDu.o
	gcc buNeDu.c -o buNeDu

buNeDu.o:
	gcc $(CFLAGS) buNeDu.c

clean: 
	rm *.o