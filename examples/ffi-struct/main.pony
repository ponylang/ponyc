use "path:./" // adds path to find shared libraries.
use "lib:ffi-struct"

use @modify_via_outer[None](s: Outer)
use @modify_inner[None](s: Inner)

actor Main
  new create(env: Env) =>
    let s: Outer = Outer

    // When passing a Pony struct in a FFI call, you get a pointer to a
    // C struct on the other side.
    @modify_via_outer(s)
    env.out.print(s.inner_embed.x.string()) // Prints 10
    env.out.print(s.inner_var.x.string()) // Prints 15

    // The syntax is the same for embed and var/let fields.
    @modify_inner(s.inner_embed)
    @modify_inner(s.inner_var)
    env.out.print(s.inner_embed.x.string()) // Prints 5
    env.out.print(s.inner_var.x.string()) // Prints 5
