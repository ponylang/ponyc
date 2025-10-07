# producer-consumer

A program showing the producer–consumer problem 
(also called the bounded-buffer problem) solved with actors.

Since producer-consumer is actually a family of problems[1], 
let us define more precisely which one is addressed here.

Specifically, here we want a single producer
that writes to a bounded-length buffer, and
we want a single consumer that reads from that 
buffer.

Our task is to coordinate the reading and 
writing so that the sequence of
reads occurs in the same order that the writes 
were made. 

That is the problem. 

* ordering invariant, the correctness property

In a correct solution, we want the 
consumer's read sequence to
be invariant with respect to the presence or
absense of the buffer. The consumer should consume in the 
same order as the producer sent output. This
is the ordering invariant. It will be 
referred to again below. 

The ordering invariant says that the buffer should be 
invisible. It says that the delivery should be the same 
whether or not the buffer is present. It says that
the buffer is just a performance optimization, and
not a logical necessity. While true, buffering
is critically important for real systems, and
is a natural model for a network link.

To provide more context and background:

The buffer is there to de-couple the producer
and consumer, allowing them to run independently 
most of the time.

They can each go at their own speed. They can 
avoid sharing a clock. The buffer can be
the only synchronization point between the two.

Having this decoupling is critical for many programs, and
the ever present reality in real networks of
independently acting computers.

The buffer prevents artificial caps on 
the rate of production and consumption. 

If the consumer is not ready
at this moment, the producer can still produce,
up to a point (the size of the buffer).

Similarly, the consumer can consume even if the
producer is now busy elsewhere, producing the
next product.

Once the ordering invariant above is met, the challenges are:

- to maximize throughput;

- while avoiding overwhelming either end.

This is a classic mutual exclusion problem that led
Dijkstra to invent the semaphore abstraction in 1965[2].

He borrowed the name from a type of colorful rigid flag arm 
used to control train crossings since the 1840s[3].

Semaphores are not in Pony, because they use locks.

The challenge: how can we solve the problem without locks?

[1] https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem

[2] https://en.wikipedia.org/wiki/Semaphore_(programming)

[3] https://en.wikipedia.org/wiki/Railway_semaphore_signal

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

Once `producer-consumer` has been compiled, in the same directory as this README file run `./producer-consumer`. You should a see of a log like:

```console
$ ./producer-consumer
test: fifo/basic apply() called.
1 test started, 0 complete: fifo/basic started
fifo: created with capacity: 2 and size: 2
consumer: started. _next = 0
producer: created. will produce 3
fifo: consumerRequestsNext: nothing for consumer, add them to _waitQcons
producer: started. _next = 0
fifo: requestToProduce(next=0) allows producer to produce id = 0 since _buf.size = 2 and _promised = 0 together are < _cap == 2
producer: has produced 0
fifo: dispatch() is taking consumer off _waitQcons to provide them next = 0
fifo: requestToProduce(next=1) allows producer to produce id = 1 since _buf.size = 2 and _promised = 0 together are < _cap == 2
consumer: has consumed 0
producer: has produced 1
consumer: about to ask for _next = 1
fifo: requestToProduce(next=2) allows producer to produce id = 2 since _buf.size = 2 and _promised = 0 together are < _cap == 2
fifo: consumerRequestsNext() has _buf.size = 2
fifo: consumerRequestsNext about to provide = 1 ; now _buf.size = 2
producer: has produced 2
consumer: has consumed 1
consumer: about to ask for _next = 2
fifo: consumerRequestsNext() has _buf.size = 2
fifo: consumerRequestsNext about to provide = 2 ; now _buf.size = 2
consumer: has consumed 2
elapsed nanosec = 186000
1 test started, 1 complete: fifo/basic complete
---- Passed: fifo/basic
----
---- 1 test ran.
---- Passed: 1
```

## Program Modifications

### Basic

1. Read the assertions. Do they assert that the
consumer never consumes the same product twice?
If not, add an assertion to this effect.

2. Verify that the ordering invariant is maintained
in this code. Can we assert this in other places?

In other words, assert that the reader consumes products
in the same sequential order that the producer produced them.

(Formally, the consumption and production sequences should map one-to-one 
to the natural numbers.)

3. Try adding more than one consumer. How do the invariants change?

4. Try adding more than one producer. How do the invariants change?

### Medium

5. Try sending and reading in batches, rather than one by one.

6. Try having an unbounded buffer rather than a fixed size one.

7. Cancellation

How do we avoid leaking memory or queue resources
if the producer or consumer wants to depart and
abandon its half-started consume or produce?

Implement a mechanism for clean departure, including
a means for both the producer and the consumer
to get acknowledgement that the buffer has
actually cancelled all outstanding requests.

What is the hazard in a real system if this
feature is missing? Hint: why does TCP
have the TIME_WAIT state?

### Advanced

8. Optimistic batching

Rather than asking for permission, what if the
system was optimistic, and then asked for forgiveness?

That is, could the system be optimized by having the producer
send a batch of everything it has on hand and letting the buffer discard
what it cannot take, since its information about
how many slots are free might be stale by the time 
the product batch arives?

This is based on the observation that there might be more
free slots now, since the consumer might done alot of
consuming since the original produce permission was granted.

This tends to happen especially in networks with large
bandwidth-delay products, like satellite links.
There, WAN network protocols like UDT[4], deploy this 
optimization. But it can happen in any parallel setting.

How would the buffer communicate back how many it 
could not store? This would require the producer
to have its own in-actor buffer. Would this extra memory
be worth it? What would you measure to tell?
Try it and find out.

9. Cutting down on communication

Pony's iso references means that the buffer might actually
be owned by the consumer or producer, rather than
in a third actor (the buffer). 

Would there be benefit to trying to "cut out the middle man", and
directly communicate between only two actors instead of three?

Implement this, and measure if it yields higher throughtput.
## Questions to think about

What is the optimal buffer size? 

Does it depend on which is faster, the consumer or the producer?

If the buffer size is unbounded, under what circumstances
should we expect the kernel to OOM kill our program for
running out of memory? 

If we provided an unbounded buffer, how else 
can we prevent a fast producer from using all
available memory?

After thinking about it, you might look into 
how Pony implements back-pressure
for its runtime system of actors with unbounded
message queues.

[4] https://en.wikipedia.org/wiki/UDP-based_Data_Transfer_Protocol
