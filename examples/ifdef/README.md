# ifdef

A program showing use of compile-time build flags.

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
Writing ./ifdef.o
Linking ./ifdef
```

## How to Run

Once `ifdef` has been compiled, in the same directory as this README file run `./ifdef`. You should see three lines of output (yours may differ from below).

```console
$ ./ifdef
Hello Linux
4
Not failing
```

## Program Modifications

As this program is showing compile-time build flags, you should leave the program as-is, but modify how you build the program.

Try enabling the "foo" build flag to change the middle line of output.

```console
$ ponyc -D=foo
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
Writing ./ifdef.o
Linking ./ifdef
```

```console
$ ./ifdef
Hello Linux
3
Not failing
```

Try compiling with the "fail" build flag to see what a `compile_error` looks like.

```console
$ ponyc -D=fail
...
Generating
 Reachability
 Selector painting
 Data prototypes
 Data types
 Function prototypes
 Functions
Error:
/home/ryan/dev/github.com/ponylang/ponyc/examples/ifdef/ifdef.pony:39:7: compile error: We don't support failing
      compile_error "We don't support failing"
      ^
```
