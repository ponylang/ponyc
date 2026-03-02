"""
Regression test for type parameter reification with three levels of nesting:
generic method -> object literal -> object literal -> lambda. This creates
three levels of type parameter copies, testing that reify_typeparamref follows
the ast_data chain all the way to the root, not just one or two levels.

See also: lambda-in-object-in-generic-method
"""
use @pony_exitcode[None](code: I32)

primitive Processor
  fun transform[U: Any val](arr: Array[U32] val,
    fn: {(U32): U} box): Array[U]
  =>
    let result = Array[U](arr.size())
    // Level 1: object literal inside generic method
    let outer =
      object
        fun process(a: Array[U32] val, f: {(U32): U} box, out: Array[U]) =>
          // Level 2: another object literal
          let inner =
            object
              fun apply_all(a': Array[U32] val, f': {(U32): U} box,
                out': Array[U])
              =>
                for v in a'.values() do
                  // Level 3: lambda inside object inside object inside
                  // generic method - three levels of type param copying
                  let apply_fn = {(x: U32): U => f'(x) }
                  out'.push(apply_fn(v))
                end
            end
          inner.apply_all(a, f, out)
      end
    outer.process(arr, fn, result)
    result

actor Main
  new create(env: Env) =>
    let arr: Array[U32] val = [5; 10; 15]
    let result = Processor.transform[U32](arr, {(n: U32): U32 => n * 2})
    try
      if result(0)? != 10 then error end
      if result(1)? != 20 then error end
      if result(2)? != 30 then error end
      @pony_exitcode(1)
    end
