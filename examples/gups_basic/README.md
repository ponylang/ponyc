# gups_basic

A program which runs a benchmark calculating GUPS (Giga updates per second) -- a random access metric.

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
Writing ./gups_basic.o
Linking ./gups_basic
```

## How to Run

Once `gups_basic` has been compiled, in the same directory as this README file run `./gups_basic`. You should see a message containing the elapsed time and GUPS (Giga updates per second) for the run. The settings of a run are configurable by CLI arguments (see `./gups_basic --help` for details).

```console
$ ./gups_basic
Time: 0.594845 GUPS: 0.0688583
```

## Program Modifications

This program is a benchmark and should be read rather than modified as a learning exercise.
