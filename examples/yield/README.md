# yield

A program showing a pattern in Pony to transform for/while loops (which could run infinitely) into tail-recursive behavior calls -- the advantage of this beyond avoiding a non-yielding actor is that garbage collection occurs between behaviors.

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
Writing ./yield.o
Linking ./yield
```

## How to Run

Once `yield` has been compiled, in the same directory as this README file run `./yield`. You should see a message showing the "infinite" actor has started, followed by the result of the kill command.

```console
$ ./yield
Beep boop!
Ugah!
```

This program has multiple "sub-programs" in it which you can see with `--help`.

```console
$ ./yield --help
usage: yield [<options>] [<args> ...]

Demonstrate use of the yield behaviour when writing tail recursive
behaviours in pony.

By Default, the actor will run quiet and interruptibly.

Options:
   -b, --bench=0         Run an instrumented behaviour to guesstimate overhead of non/interruptive.
   -d, --debug=false     Run in debug mode with verbose output.
   -h, --help=false
   -l, --lonely=false    Run a non-interruptible behaviour with logic that runs forever.
   -p, --punk=false      Run a punctuated stream demonstration.
```

Try running each of these program, but beware `--lonely` will run forever so you will need to externally kill it.

## Program Modifications

Modify the program to make each of the above options into (leaf) subcommands by making the existing "yield" a parent command. `debug` should be the only subcommand which takes an argument.

```console
$ ./yield bench --perf=10
...
```

```console
$ ./yield debug
...
```

```console
$ ./yield lonely
...
```

```console
$ ./yield punk
...
```
