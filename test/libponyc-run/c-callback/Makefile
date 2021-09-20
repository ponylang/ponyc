program: additional.so main.pony
	ponyc --verbose 0 --bin-name program

additional.so: additional.o
	cc -shared -o libadditional.so additional.o

additional.o: additional.c
	cc -o additional.o -c additional.c

clean:
	rm -f additional.o libadditional.so program.o program

.PHONY: clean
