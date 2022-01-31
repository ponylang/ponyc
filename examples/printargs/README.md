# printargs

A program showing use of the `cli` package to read/print commandline arguments and environment variables.

## How to Compile

With a minimal Pony installation, in the same directory as this README file run `ponyc`. You should see content building the necessary packages, which ends with:

```console
...
Generating
 Reachability
 Selector painting
 Data prototypes
 Data types
 Function prototypes
 Functions
 Descriptors
Optimising
Writing ./printargs.o
Linking ./printargs
```

## How to Run

Once `printargs` has been compiled, in the same directory as this README file run `./printargs`. You should an echo of your command along with arguments followed by your environment variables. (You will likely have far more environment variables than what is shown below.)

```console
$ ./printargs
./printargs This is a test
...
SHELL = /bin/bash
USER = ryan
HOME = /home/ryan
LANGUAGE = en_US
...
```

## Program Modifications

Modify the program to take commands which print only arguments or environment variables.

```console
$ ./printargs args This is a test
./printargs args This is a test
```

```console
$ ./printargs env
...
SHELL = /bin/bash
USER = ryan
HOME = /home/ryan
LANGUAGE = en_US
...
```
