# commandline

A naive `echo` program which repeats its arguments, optionally uppercase words.

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
Writing ./commandline.o
Linking ./commandline
```

## How to Run

Once `commandline` has been compiled, in the same directory as this README file run `./commandline Example words to echo`. You should see your same message repeated back to you:

```console
$ ./commandline Example words to echo
Example words to echo
```

We can access a help message by using `--help`.

```console
$ ./commandline --help
usage: echo [<options>] <words>

A sample echo program

Options:
   -h, --help=false     
   -U, --upper=false    Uppercase words

Args:
   <words>    The words to echo
```

From here we can see that there is an option to uppercase words (`-U`).

```console
$ ./commandline -U Example words to echo
EXAMPLE WORDS TO ECHO 
```
