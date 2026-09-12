# files

A program which shows the basic usage of the "files" package by reading the path of the file and its contents.

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
Writing ./files.o
Linking ./files
```

## How to Run

Once `files` has been compiled, in the same directory as this README file run `./files files.pony`. You should see the absolute path of the `files.pony` source file, as well as its contents.

```console
$ ./files files.pony
/home/ryan/dev/github.com/ponylang/ponyc/examples/files/files.pony
use "files"

actor Main
  new create(env: Env) =>
...
    else
      try
        env.out.print("Couldn't open " + env.args(1)?)
      end
    end
```

## Program Modifications

Modify the program to print the file contents with line numbers.

```console
$ ./files files.pony
/home/ryan/dev/github.com/ponylang/ponyc/examples/files/files.pony
1 use "files"
2
3 actor Main
4   new create(env: Env) =>
...
16    else
17      try
18        env.out.print("Couldn't open " + env.args(1)?)
19      end
20    end
```
