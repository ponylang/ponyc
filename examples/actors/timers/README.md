# timers

A program showing use of `Timer` to track time via behaviors.

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
Writing ./timers.o
Linking ./timers
```

## How to Run

Once `timers` has been compiled, in the same directory as this README file run `./timers`. You should see a message about one timer being canceled early, the other will continue running.

```console
$ ./timers
timer cancelled
timer: 1
timer: 2
...
timer: 8
timer: 9
timer: 10
timer cancelled
```

## Program Modifications

Modify this program to make the count correspond to seconds passed. Hint: count corresponds to the number of times a given `Timer` has been called.
