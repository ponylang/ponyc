# echo

A simple server which echos back the data that it is sent.

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
Writing ./echo.o
Linking ./echo
```

## How to Run

Once `echo` has been compiled, in the same directory as this README file run `./echo`. You should see a message containing your local address and a randomly selected port number:

```console
$ ./echo
listening on 127.0.0.1:40503
```

If you visit this address using a web browser, you will see further output:

```console
$ ./echo
listening on 127.0.0.1:40503
connection accepted
data received, looping it back
...
```

## Program Modifications

TODO: Add content here
