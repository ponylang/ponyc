# mixed

A program simulating rings of interconnected actors passing messages.

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
Writing ./mixed.o
Linking ./mixed
```

## How to Run

Once `mixed` has been compiled, in the same directory as this README file run `./mixed`. You should see a message showing the order of arguments -- number of rings, number of actors in each rin, number of messages to pass, and number of times to repeat.

```console
$ ./mixed
mixed OPTIONS
  N   number of rings
  N   number of actors in each ring
  N   number of messages to pass around each ring
  N   number of times to repeat
```

To create a run which does work we must provide these arguments.

```console
./mixed 100 50 25 10
```

The above run has 100 rings with 50 actors in each passing 25 messages 10 times. You will notice there is no output to the console from this program.

## Program Modifications

Modify the program to use default values when no arguments are passed, but fail if non-numeric arguments are passed.

```console
./mixed
```

```console
./mixed 100 50 25 nonsense
Error: "nonsense" is not a valid arguement for number of times to repeat.
mixed OPTIONS
  N   number of rings
  N   number of actors in each ring
  N   number of messages to pass around each ring
  N   number of times to repeat
```
