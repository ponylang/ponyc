# mailbox

A program which stress tests Pony's mailbox system.

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
Writing ./mailbox.o
Linking ./mailbox
```

## How to Run

Once `mailbox` has been compiled, in the same directory as this README file run `./mailbox`. You should the usage statement showing that the program requires positional arguments.

```console
$ ./mailbox
mailbox OPTIONS
  N   number of sending actors
  M   number of messages to pass from each sender to the receiver
```

If we instead run this program with positional arguments we will see no output, but it will take time to run.

```console
./mailbox 100 1000000
```

Try setting both positional arguments high enough that you have time to observe how memory usage changes while the program is running.

## Program Modifications

Modify the program to respond to every 1000 "pongs" by telling the user the current number of "pongs" processed.

```console
$ ./mailbox 100 1000000
Processed 1000 pongs so far.
Processed 2000 pongs so far.
...
Processed 100000000 pongs so far.
```
