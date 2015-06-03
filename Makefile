all: 

	gcc -g -c -I/usr/include/postgresql serv1.c
	gcc -pthread -o serv1 serv1.o -L/usr/lib -lpq

	gcc -g -c -I/usr/include/postgresql serv2.c
	gcc -pthread -o serv2 serv2.o -L/usr/lib -lpq

	gcc -g client.c -o client

	gcc -pthread -g loadbalancer.c -o loadbalancer

clean: 	rm -f serv1.o
		rm -f serv2.o
		rm -f client
		rm -f loadbalancer
