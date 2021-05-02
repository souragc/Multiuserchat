all: server.c
	gcc server.c -o server

clean:
	rm server
