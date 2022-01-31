# ffi-struct

This program shows how to use the Pony `struct` type, as well as passing them to C functions. In this example, the `Main` actor passes both the `Outer` and `Inner` types as a struct pointer to C. The C code then modifies these structures. For more information on Pony structs, check the [Working with Structs](https://tutorial.ponylang.io/c-ffi/calling-c.html#working-with-structs-from-pony-to-c) section of the C-FFI tutorial.

## How to Compile

The first step is to compile our C code to a shared library. For this example, we're using `clang` on macOS. The exact details may vary when using other compilers and platforms, so be sure to check the appropriate documentation. Run the following commands in the same directory as this README:

```console
clang -fPIC -Wall -Wextra -g -c -o struct.o struct.c
clang -shared -o libffi-struct.dylib struct.o
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
Writing ./ffi-struct.o
Linking ./ffi-struct
```

## How to Run

Once `ffi-struct` has been compiled, in the same directory as this README file run `./ffi-struct`. You should see the following output:

```console
$ ./ffi-struct
10
15
5
5
```

## Program Modifications

Whenever we interface with C code, we need to tell the Pony compiler how to find and link the C libraries we're using. By default, the Pony compiler will look for libraries in standard places. In our case, since we're compiling a C library in another directory, we will need to be explicit.

There are two ways of doing this. One is by adding a `use path:XXX` statement to our Pony code, like in our example:

```pony
use "path:./"
use "lib:ffi-struct"
```

This tells the compiler to include the current path when linking our code. Another option is to use the `--path <path>` option in the Pony compiler. Try deleting the `use "path:./"` line from the Pony code, and running `ponyc` again. The compiler should show an error about being "unable to link" our struct library.

Now try passing the `--path .` option to `ponyc` when building the code. Everything should be fine again, and by running the resulting program, you should be able to see the same output as before.

For further modification, add new C functions that take an integer and return an `Inner*` or `Outer*` struct, respectively. Then modify the Pony code so that it calls these functions and prints to screen their fields. Remember, you will need to allocate the structs in C! Hint: you can use the `NullablePointer` type to handle possible `null` pointers in Pony. You can read the [Working with Structs: from C to Pony](https://tutorial.ponylang.io/c-ffi/calling-c.html#working-with-structs-from-c-to-pony) section of the tutorial for more information.
