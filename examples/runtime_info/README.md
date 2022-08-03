# runtime_info

A program showing use of runtime stats.

## How to Compile

You will need a compiler with runtime stats enabled to get meaningful output. To compile a runtime stats enabled compiler, you can use the following command: `make config=release use=runtimestats_messages`.

In the same directory as this README file run `ponyc` that has runtime stats enabled. You should see content building the necessary packages, which ends with:

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
Writing ./runtime_info.o
Linking ./runtime_info
```

## How to Run

Once `runtime_info` has been compiled, in the same directory as this README file run `./runtime_info`. You should see a large amount of output of actor and scheduler stats ending with something similar to the following:

```console
$ ./runtime_info
<SNIPPED>
  memory allocated influght messages: 96
  number of inflight messages: 3
```
