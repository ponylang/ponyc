program: try.so main.pony
	ponyc --verbose 0 --bin-name program

try.so: try.o
	c++ -shared -o libtry.so try.o

try.o: try.cc
	c++ -fPIC -o try.o -c try.cc

clean:
	rm -f try.o libtry.so program.o program

.PHONY: clean
