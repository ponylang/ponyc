# fan-in

A program simulating thundering herd/fan-in type workloads and how the Pony runtime backpressure impacts such workloads.

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
Writing ./fan-in.o
Linking ./fan-in
```

## How to Run

Once `fan-in` has been compiled, in the same directory as this README file run `./fan-in`. You should see a message containing the number of senders, analyzers, analyzer-interval, analyzer-report-count, and receiver-workload -- all configurable by CLI arguments (see `./fan-in --help` for details) -- followed by the timestamp, the run time in nanoseconds, and the rate of work. Let the program run to the end of message sending.

```console
$ ./fan-in
# senders 100, analyzers 1000, analyzer-interval 100, analyzer-report-count 10, receiver-workload 10000
time,run-ns,rate
...
Done with message sending... Waiting for Receiver to work through its backlog...
```

The rate of work should drop occasionally -- this is the backpressure system catching up with work from the thundering herd/fan-in. If the rate does not drop, try changing the CLI arguments to increase the workload.

## Program Modifications

Modify the program to print a message when backpressure is being experienced.

```console
$ ./fan-in
# senders 100, analyzers 1000, analyzer-interval 100, analyzer-report-count 10, receiver-workload 10000
time,run-ns,rate
...
1627792832506124895,9999369982,24400146
1627792842505604411,9999479516,122  # Backpressure experienced
1627792852503830247,9998225836,23161873
...
Done with message sending... Waiting for Receiver to work through its backlog...
```
