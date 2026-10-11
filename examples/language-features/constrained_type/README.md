# constrained_type

Demonstrates the basics of using the `constrained_types` package to encode domain type constraints such as "number less than 10" in the Pony type system.

The example program implements a type for usernames that requires that a username is between 6 and 12 characters long and only contains lower case ASCII characters.

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
Writing ./constrained_type.o
Linking ./constrained_type
```

## How to Run

Once `constrained_type` has been compiled, in the same directory as this README file run `./constrained_type A_USERNAME`. Where `A_USERNAME` is a string you want to check to see if it meets the business rules above.

For example, if you run `./constrained_type magenta` you should see:

```console
$ ./constrained_type magenta
magenta is a valid username!
```
