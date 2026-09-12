"""
Example of the syntax used to pass structs in FFI calls.
First, build the C file as a library, then build the Pony
program.

How to make shared library:
gcc -c -Wall -Werror -fpic struct.c
gcc -shared -o libffi-struct.so struct.o
"""

// Unlike classes, Pony structs have the same binary layout as C structs and
// can be transparently used in C functions.
// Structs do not have a type descriptor, which means they cannot be used in
// algebraic types or implement traits/interfaces.
struct Inner
  """
  A simple struct with a single integer field.
  """
  var x: I32 = 0

// An embed field is embedded in its parent object, like a C struct inside
// C struct.
// A var/let field is a pointer to an object allocated separately.
struct Outer
  """
  Contains an Inner by embedding and another by pointer.
  """
  embed inner_embed: Inner = Inner
  var inner_var: Inner = Inner
