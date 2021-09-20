program: finaliser.so main.pony
	ponyc --verbose 0 --bin-name program

finaliser.so: finaliser.o
	cc -shared -o libfinaliser.so finaliser.o

finaliser.o: finaliser.c
	cc -o finaliser.o -c finaliser.c

clean:
	rm -f finaliser.o libfinaliser.so program.o program

.PHONY: clean
