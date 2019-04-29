CFLAGS= -Wall -pedantic-errors -std=c11

all: Banka client

Banka:
	gcc $(CFLAGS) bank.c -o Banka -lrt

client:
	gcc $(CFLAGS) client.c -o Client -lrt

clean:
	rm Banka Client
