bg_shell : main.o
	cc -o bg_shell main.o

main.o : src/main.c
	cc -c src/main.c

clean :
	rm bg_shell main.o