all: server client

server: server.cpp
	g++ -Wall server.cpp -o server
    
client: client.c
	gcc -Wall client.c -o client
    
.PHONY: clean 

clean:
	rm -f server client