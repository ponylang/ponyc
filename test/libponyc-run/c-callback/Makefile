program: callback.so main.pony
	ponyc --verbose 0 --bin-name program

callback.so: callback.o
	cc -shared -o libcallback.so callback.o

callback.o: callback.c
	cc -o callback.o -c callback.c

clean:
	rm -f callback.o libcallback.so program.o program

.PHONY: clean
