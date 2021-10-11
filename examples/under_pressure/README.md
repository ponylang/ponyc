# under_pressure

A program showing how to utilize the Pony backpressure system.

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
Writing ./under_pressure.o
Linking ./under_pressure
```

## How to Run

Once `under_pressure` has been compiled, in the same directory as this README file run `./under_pressure`. You should see a message reporting a connection failure.

```console
$ ./under_pressure
connect_failed
```

This failure is because there is no receiver for the data `under_pressue` is receiving. You can start one with `netcat`.

```console
# Data receiver
nc -l 127.0.0.1 7669 >> /dev/null
```

Now run `under_pressure` (your output w)

```console
$ ./under_pressure
Querying and setting socket options:
        getsockopt so_error = 0
        getsockopt get_tcp_nodelay = 0
        getsockopt set_tcp_nodelay(true) return value = 13
        getsockopt get_tcp_nodelay = 0
        getsockopt rcvbuf = 131072
        getsockopt sndbuf = 2626560
        setsockopt rcvbuf 5000 return was 0
        setsockopt sndbuf 5000 return was 0
```

If you then stop the `netcat` process you will see `under_pressure` respond that is it experiencing backpressure. Restart `netcat` and `under_pressure` will release backpressure. If you look at a system monitor while under backpressure you will notice `under_pressure` has very low usage -- no actors are running!

## Program Modifications

Modify the program to use more than one `TCPConnection` -- all actors will respond to backpressure on its own.
