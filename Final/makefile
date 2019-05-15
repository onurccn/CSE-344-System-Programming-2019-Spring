CFLAGS= -Wall -pedantic-errors -std=c11

all: server client

server:
	gcc $(CFLAGS) server.c -o server -lpthread

client:
	gcc $(CFLAGS) client.c -o client

clean:
	rm server client
