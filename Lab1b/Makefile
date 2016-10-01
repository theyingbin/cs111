default: client.c server.c
	gcc -lpthread -o client -lmcrypt client.c
	gcc -lpthread -o server -lmcrypt server.c
clean:
	rm client
	rm server
dist:
	tar -czf lab1b-104485521.tar.gz client.c server.c my.key Makefile README
