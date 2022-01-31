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
...
```

Your program is now listening for any message sent to your local address on the selected port number. To check the program is working as intended, you should open a second terminal window and run the following `netcat` command matching the local address and port from the `echo` program:

```console
$ nc 127.0.0.1 40503
...
```

This gives us a client which we can use to send `echo` a message. try typing any message and the `echo` program will echo it back to you.

```console
$ nc 127.0.0.1 40503
Hello world!
server says: Hello world!
...
```

## Program Modifications

Modify your program to send a welcome message to a user which provides instructions on interacting with the server.

```console
$ ./echo
listening on 127.0.0.1:40503
...
```

```console
$ nc 127.0.0.1 40503
Welcome! Write your message below then hit the Enter key.
Hello world!
server says: Hello world!
...
```
