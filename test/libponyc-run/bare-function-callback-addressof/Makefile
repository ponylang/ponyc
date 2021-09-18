clean:
	rm -f callbacks.o libcallbacks.so program.o program

callbacks.so: callbacks.o
	cc -shared -o libcallbacks.so callbacks.o

callbacks.o: callbacks.c
	cc -o callbacks.o -c callbacks.c

program: callbacks.so main.pony
	ponyc --bin-name program

.PHONY: clean
