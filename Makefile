bg_shell : main.o str.o job.o
	cc -o bg_shell main.o str.o job.o

main.o : src/main.c
	cc -c src/main.c

str.o : src/str.c src/str.h
	cc -c src/str.c

job.o : src/job.c src/job.h
	cc -c src/job.c

clean :
	rm bg_shell main.o