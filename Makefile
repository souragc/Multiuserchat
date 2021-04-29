all: server.c client.c
	gcc server.c -o server

clean:
	rm server
