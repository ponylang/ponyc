# message-ubench

A program simulating high message passing workloads and how the Pony runtime throughput responds to such workloads.

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
Writing ./message-ubench.o
Linking ./message-ubench
```

## How to Run

Once `message-ubench` has been compiled, in the same directory as this README file run `./message-ubench`. You should see a message containing the number of pingers, report interval, report count, and initial pings -- all configurable by CLI arguments (see `./message-ubench --help` for details) -- followed by the timestamp, the run time in nanoseconds, and the rate of work. By default this program run infinitely so you will need to kill/cancel the program at some point to proceed.

```console
$ ./message-ubench
# pingers 8, report-interval 10, report-count 0, initial-pings 5
time,run-ns,rate
1629166694.245655917,999491638,18797083
1629166695.244990149,999289492,21825087
...
```

To create a run which does not run infinitely, you need to modify `--report-count` to define how many reports to generate before exiting.

```console
$ ./message-ubench --report-count 5
# pingers 8, report-interval 10, report-count 0, initial-pings 5
time,run-ns,rate
1629167112.067643792,999387696,18909804
1629167113.066978277,999285041,23451868
1629167114.066308216,999281458,23511351
1629167115.065636018,999293718,23549719
1629167116.064974250,999304040,24435030
```

## Program Modifications

Modify the program to change the number of messages "in flight" at any one time by causing Pingers to send two messages to their evenly indexed neighbors and no messages to their oddly indexed neighbors. Hint: modify `send_pings()` and use `(n % 2) == 0` to determine how many messages to send to neighbor `n`.

```console
$ ./message-ubench --report-count 5
# pingers 8, report-interval 10, report-count 5, initial-pings 5
time,run-ns,rate
1629168710.593900613,999775030,412
1629168711.593643741,999427681,2593
1629168712.592208576,998285666,9065
1629168713.591799316,999430123,2883
1629168714.591319039,999361155,7306
```
