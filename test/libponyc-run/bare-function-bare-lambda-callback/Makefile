program: callbacks.so main.pony
	ponyc --verbose 0 --bin-name program

callbacks.so: callbacks.o
	cc -shared -o libcallbacks.so callbacks.o

callbacks.o: callbacks.c
	cc -o callbacks.o -c callbacks.c

clean:
	rm -f callbacks.o libcallbacks.so program.o program

.PHONY: clean
