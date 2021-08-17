# gups_opt

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
Writing ./gups_opt.o
Linking ./gups_opt
```

## How to Run

Once `gups_opt` has been compiled, in the same directory as this README file run `./gups_opt`. You should see a message containing the settings, elapsed time, and GUPS (Giga updates per second) for the run. The settings of a run are configurable by CLI arguments (see `./gups_opt --help` for details).

```console
$ ./gups_opt
logtable: 20
iterate: 10000
logchunk: 10
logactors: 2
Time: 0.114243
GUPS: 0.358534
```

## Program Modifications

This program is a benchmark and should be read rather than modified as a learning exercise.
