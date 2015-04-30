all:
	gcc -Wall -fPIC -DPIC -c faketime.c
	ld -shared -o faketime.so faketime.o -ldl
	gcc tdfgen.c
	rm -f tdfgen
	mv a.out tdfgen

