program: identity.so main.pony
	ponyc --verbose 0 --bin-name program

identity.so: identity.o
	cc -shared -o libidentity.so identity.o

identity.o: identity.c
	cc -o identity.o -c identity.c

clean:
	rm -f identity.o libidentity.so program.o program

.PHONY: clean
