CFLAGS=-c -ansi -Wall -pedantic-errors

all: exec clean

exec: buNeDu.o
	gcc buNeDu.c -o exec

buNeDu.o:
	gcc $(CFLAGS) buNeDu.c

clean: 
	rm *.o