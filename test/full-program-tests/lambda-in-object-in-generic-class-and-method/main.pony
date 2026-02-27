"""
Regression test for type parameter reification with a lambda inside an object
literal inside a generic method that has multiple type parameters. This tests
that all type parameters (A and B) from the method are correctly reified
through two levels of type parameter copying.

See also: lambda-in-object-in-generic-method
"""
use @pony_exitcode[None](code: I32)

primitive Mapper
  fun map[A: Any val, B: Any val](arr: Array[A] val,
    fn: {(A): B^} box): Array[B]
  =>
    let result = Array[B](arr.size())
    // Object literal inside generic method - level 1 copy of A, B
    let mapper =
      object
        fun do_map(a: Array[A] val, f: {(A): B^} box, out: Array[B]) =>
          for v in a.values() do
            // Lambda inside object inside generic method - level 2 copy
            let apply_fn = {(x: A): B^ => f(x) }
            out.push(apply_fn(v))
          end
      end
    mapper.do_map(arr, fn, result)
    result

actor Main
  new create(env: Env) =>
    let arr: Array[U32] val = [1; 2; 3]
    let result = Mapper.map[U32, String](arr,
      {(n: U32): String^ => n.string()})
    try
      if result(0)? != "1" then error end
      if result(1)? != "2" then error end
      if result(2)? != "3" then error end
      @pony_exitcode(1)
    end
