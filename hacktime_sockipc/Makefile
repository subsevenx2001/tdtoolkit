all:
	gcc -Wall -fPIC -DPIC -c faketime.c
	ld -shared -o faketime.so faketime.o -ldl
#	gcc tdfgen.c
#	rm -f tdfgen
#	mv a.out tdfgen
	gcc -std=gnu99 tdfcontroll.c
	mv a.out tdfcontroll
	gcc -std=gnu99 remote_master.c -lpcap -lpthread
	mv a.out remote_master 

install:
	cp faketime.so /lib/

clean:
	rm -f faketime.so
	rm -f faketime.o
	rm -f tdfgen
	rm -f tdfcontroll
	rm -f remote_master
	rm -f *.c~
	rm -f *.h~
