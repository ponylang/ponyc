# readline

A program showing usage of `Readline` from the `term` package.

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
Writing ./readline.o
Linking ./readline
```

## How to Run

Once `readline` has been compiled, in the same directory as this README file run `./readline`. You should see an brief message about how to exit followed by an interactive prompt. Use "quit" to exit.

```console
$ ./readline
Use 'quit' to exit.
0 > This
1 > is
2 > a
3 > test
4 > quit
```

## Program Modifications

In addition to the "quit" command, add commands such as "count" which prints the current count number and "history" which prints the history of commands entered.

```console
$ ./readline
Use 'quit' to exit.
0 > This
1 > is
2 > a
3 > test
4 > count
Current count is: 4
5 > history
Commands:
  This
  is
  a
  test
  count
6 > history
Commands:
  This
  is
  a
  test
  count
  history
7 > quit
```
