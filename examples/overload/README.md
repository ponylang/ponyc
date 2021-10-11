# overload

A program simulating a single actor, `Receiver`, being overloaded with messages from `Sender`s. The Pony backpressure system prevents runaway memory growth by preventing `Sender`s from being scheduled until the `Receiver` is no longer overloaded.

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
Writing ./overload.o
Linking ./overload
```

## How to Run

Once `overload` has been compiled, in the same directory as this README file run `./overload`. You should see a report of the number of senders, the single receiver starting, then messages about the number of messages received in a given timeframe. (This program will loop infinitely so you will need to force it to step.)

```console
$ ./overload
Starting 10000 senders.
Single receiver started.
50000000 messages received in 10761731202 nanos.
100000000 messages received in 12065642226 nanos.
150000000 messages received in 13481541604 nanos.
...
```

## Program Modifications

Modify the program to allow providing the number of senders at the commandline interface.

```console
$ ./overload 2000000
Starting 2000000 senders.
Single receiver started.
50000000 messages received in 9110934011 nanos.
100000000 messages received in 9233131150 nanos.
150000000 messages received in 9110256111 nanos.
...
```
