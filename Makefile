build:
	gcc -Wall server.c db.c -o server.out -pthread -lsqlite3

server: build
	./server.out

client:
	python3 client.py
