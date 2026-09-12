# ring

A program showing rings of actors passing messages.

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
Writing ./ring.o
Linking ./ring
```

## How to Run

Once `ring` has been compiled, in the same directory as this README file run `./ring`. You should see a single number printed.

```console
$ ./ring
2
```

What this program is doing is setting up a ring of actors (each with an ID starting at 1), passing a set of messages, and the final actor to receive the message prints its ID.

This program has arguments which can be seen using `--help`.

```console
$ ./ring --help
rings OPTIONS
  --size N number of actors in each ring
  --count N number of rings
  --pass N number of messages to pass around each ring
```

As stated above, `--size` changes the number of actors in a ring, `--count` changes the number of rings, and `--pass` changes the number of messages to pass. By default `--size` is 3, `--count` is 1, and `--pass` is 10.

## Program Modifications

Notice something about this program: any random argument will display the "help message" such as `./ring --this-is-not-an-argument`. Modify the program to use the `cli` package so that it has a defined `--help` command and rejects such random arguments.
