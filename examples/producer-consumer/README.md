# producer-consumer

A program showing the producer–consumer problem (also called the bounded-buffer problem) solved with actors.

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
Writing ./producer-consumer.o
Linking ./producer-consumer
```

## How to Run

Once `producer-consumer` has been compiled, in the same directory as this README file run `./producer-consumer`. You should a set of messages from: Main, Buffer, Producer, and Consumer actors.

```console
$ ./producer-consumer
**Main** Finished.
**Buffer** Permission_to_produce
**Buffer** Permission_to_produce: Calling producer to produce
...
**Buffer** Store_product
**Buffer** Store_product: Calling consumer to consume
**Consumer** Consuming product 2
```

## Program Modifications

Rather than modifying this program, pay attention to how the use of behaviors means this program is non-blocking and does not use locks/semaphores.
