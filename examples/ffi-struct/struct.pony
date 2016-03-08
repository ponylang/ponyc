"""
Example of the syntax used to pass structs in FFI calls.
First, build the C file as a library, then build the Pony program.
"""

use "lib:ffi-struct"

use @modify_via_outer[None](s: Outer)
use @modify_inner[None](s: Inner)

// Unlike classes, Pony structs have the same binary layout as C structs and
// can be transparently used in C functions
struct Inner
  var x: I32 = 0

// An embed field is embedded in its parent object, like a C struct inside
// C struct.
// A var/let field is a pointer to an object allocated separately.
struct Outer
  embed inner_embed: Inner = Inner
  var inner_var: Inner = Inner

actor Main
  new create(env: Env) =>
    let s = Outer

    // When passing a Pony struct in a FFI call, you get a pointer to a
    // C struct on the other side.
    @modify_via_outer(s)
    env.out.print(s.inner_embed.x.string()) // Prints 10
    env.out.print(s.inner_var.x.string()) // Prints 15

    // The syntax is the same for embed and var/let fields
    @modify_inner(s.inner_embed)
    @modify_inner(s.inner_var)
    env.out.print(s.inner_embed.x.string()) // Prints 5
    env.out.print(s.inner_var.x.string()) // Prints 5
