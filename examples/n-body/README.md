# n-body

A program representing an N-body simulation of five planetary bodies: Sun, Jupiter, Saturn, Uranus, and Neptune.

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
Writing ./n-body.o
Linking ./n-body
```

## How to Run

Once `n-body` has been compiled, in the same directory as this README file run `./n-body`. You should a measurement of the initial energy of the system. The program will continue running for a set number of cycles (by default, 50000000) then print the resulting energy of the system.

```console
$ ./n-body
-0.169075
-0.16906
```

To create a run which runs for a different number of cycles, provide a numeric argument.

```console
./n-body 100000000
-0.169075
-0.169035
```

The above run doubles the default number of cycles.

## Program Modifications

Modify the program to make `Body` a trait/interface and create classes for `Sun`, `Jupiter`, `Saturn`, `Uranus`, and `Neptune` which satisfy `Body`.
