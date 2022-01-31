# ffi-callbacks

This program shows how to pass Pony functions as callbacks to C functions. In this example, the `Main` actor uses two ways to do so: either via bare functions, or using bare lambdas. The C code then calls this callback with some hard-coded arguments. For more information on bare functions and bare lambdas, check the [Callbacks](https://tutorial.ponylang.io/c-ffi/callbacks.html) section of the C-FFI tutorial.

## How to Compile

The first step is to compile our C code to a shared library. For this example, we're using `clang` on macOS. The exact details may vary when using other compilers and platforms, so be sure to check the appropriate documentation. Run the following commands in the same directory as this README:

```console
clang -fPIC -Wall -Wextra -g -c -o callbacks.o callbacks.c
clang -shared -o libffi-callbacks.dylib callbacks.o
```

Now we should be ready to compile our Pony code. With a minimal Pony installation, in the same directory as this README file run `ponyc`. You should see content building the necessary packages, which ends with:

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
Writing ./ffi-callbacks.o
Linking ./ffi-callbacks
```

## How to Run

Once `ffi-callbacks` has been compiled, in the same directory as this README file run `./ffi-callbacks`. You should see the number `10` printed three times:

```console
$ ./ffi-callbacks
10
10
10
```

## Program Modifications

Whenever we interface with C code, we need to tell the Pony compiler how to find and link the C libraries we're using. By default, the Pony compiler will look for libraries in standard places. In our case, since we're compiling a C library in another directory, we will need to be explicit.

There are two ways of doing this. One is by adding a `use path:XXX` statement to our Pony code, like in our example:

```pony
use "path:./"
use "lib:ffi-callbacks"
```

This tells the compiler to include the current path when linking our code. Another option is to use the `--path <path>` option in the Pony compiler. Try deleting the `use "path:./"` line from the Pony code, and running `ponyc` again. The compiler should show an error about being "unable to link" our callback library.

Now try passing the `--path .` option to `ponyc` when building the code. Everything should be fine again, and by running the resulting program, you should be able to see the same output as before.

For further modification, modify the Pony and C code so that you can change which number is passed to the Pony callback. You can do this by adding a new parameter to the `do_callback` function and to the `CB` type. Remember to update the FFI signature of `do_callback`!
