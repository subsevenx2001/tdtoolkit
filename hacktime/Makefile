all:
	gcc -Wall -fPIC -DPIC -c faketime.c
	ld -shared -o faketime.so faketime.o -ldl

install:
	cp faketime.so /lib/

clean:
	rm -f faketime.so
	rm -f faketime.o
