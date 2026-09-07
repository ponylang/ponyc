use "pony_check"
use @pony_exitcode[None](code: I32)

// Regression test for https://github.com/ponylang/ponyc/issues/4838
//
// When a lambda is nested inside an object literal inside a generic method,
// type parameters are copied twice by collect_type_params. The compiler's
// reify_typeparamref followed the ast_data chain only one level, failing to
// match these doubly-copied type parameters. This caused the lambda's apply
// method to not be marked as reachable, leaving a hole in the vtable and
// producing a segfault at runtime.
//
// The bug manifests when Generator.map's internal object literal wraps
// a lambda passed by the caller, creating a chain of nested object literals.
// Calling generate on the mapped generator triggers the segfault because the
// lambda's apply method has no vtable entry.

actor Main
  new create(env: Env) =>
    let gen = recover val
      Generators.u32().map[U32]({(n: U32): U32^ => n + 1})
    end
    let rnd = Randomness
    try
      // generate calls through the mapped generator's object literal,
      // which invokes the lambda — the nested pattern that triggers the bug.
      // Without the fix, this segfaults due to the missing vtable entry.
      let value: U32 = gen.generate(rnd)?
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
