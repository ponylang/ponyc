CC ?= cc
CXX ?= c++

program: additional.so main.pony
	ponyc --verbose 0 --bin-name program

additional.so: additional.o
	$(CXX) -shared -o libadditional.so additional.o

additional.o: additional.cc
	$(CXX) -fPIC -o additional.o -c additional.cc

clean:
	rm -f additional.o libadditional.so program.o program

.PHONY: clean
