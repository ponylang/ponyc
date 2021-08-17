# helloworld

A program running the classic "Hello, world." learning example.

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
Writing ./helloworld.o
Linking ./helloworld
```

## How to Run

Once `helloworld` has been compiled, in the same directory as this README file run `./helloworld`. You should see a brief hello.

```console
$ ./helloworld
Hello, world.
```

## Program Modifications

Modify the program to say hello to a specific name by first commandline argument -- when no argument is given, default to the world.

```console
$ ./helloworld Ryan
Hello, Ryan.
```

```console
$ ./helloworld
Hello, world.
```
