all: server.c client.c
	gcc server.c -o server
	gcc client.c -o client -lcrypto

clean:
	rm server
	rm client
