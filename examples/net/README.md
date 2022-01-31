# net

A program showing a Ping-Pong server.

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
Writing ./net.o
Linking ./net
```

## How to Run

Once `net` has been compiled, in the same directory as this README file run `./net`. You should see messages pertaining to clients being spawned, pings/pongs being sent, and the address/port each entity is listening on. Your output is not likely to match below, but you should see similar output.

```console
$ ./net
listening on 127.0.0.1:40199
spawn 1
Client starting
Pong: listening on 127.0.0.1:49486
Ping: listening on 127.0.0.1:51350
from 127.0.0.1:51350
ping!
Pong: closed
from 127.0.0.1:49486
pong!
Ping: closed
Server starting
connecting: 1
connected to 127.0.0.1:40199
accepted from 127.0.0.1:53680
client says hi
server says hi
client closed
server closed
```

You can control the number of clients spawned by providing a numeric argument to `net` -- you will get a message for each spawn along with similar output as before.

```console
$ ./net 5
listening on 127.0.0.1:41487
spawn 1
...
spawn 2
...
spawn 3
...
spawn 4
...
spawn 5
Client starting
accepted from 127.0.0.1:50910
client says hi
client closed
server says hi
server closed
Server starting
connecting: 1
client closed
connected to 127.0.0.1:41487
accepted from 127.0.0.1:50912
server closed
server says hi
client says hi
client closed
server closed
```

## Program Modifications

Modify the program to make use of `env.err.print` (as opposed to `env.out.print`) when things go wrong. Hint: look for uses of `try`-blocks to determine when things go wrong.
