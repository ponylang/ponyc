# spreader

A program producing a binary tree of actors at a given depth, which then counts/reports the number of actors in the tree.

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
Writing ./spreader.o
Linking ./spreader
```

## How to Run

Once `spreader` has been compiled, in the same directory as this README file run `./spreader`. You should see a message reporting the number of actors in the tree.

```console
$ ./spreader
1023 actors
```

This program can take a numeric arguments stating the depth of the tree to build (the default is 10).

```console
$ ./spreader 5
31 actors
```

## Program Modifications

Modify this program to print its total depth.

```console
$ ./spreader 5
31 actors at depth 5
```

Additionally, add a second argument for the branching factor.

```console
$ ./spreader 5 3
242 actors at depth 5 with branch factor of 3
```
