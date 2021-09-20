program: additional.so main.pony
	ponyc --verbose 0 --bin-name program

additional.so: additional.o
	c++ -shared -o libadditional.so additional.o

additional.o: additional.cc
	c++ -fPIC -o additional.o -c additional.cc

clean:
	rm -f additional.o libadditional.so program.o program

.PHONY: clean
