class Repeat[A] is Iterator[A]
  """
  Create an iterator that returns the specified value forever.

  ## Example program

  Print the number 7 forever.

  ```pony
  use "itertools"

  actor Main
    new create(env: Env) =>
      for x in Repeat[U32](7) do
        env.out.print(x.string())
      end
  ```
  """
  let _v: A

  new create(v: A) =>
    _v = consume v

  fun ref has_next(): Bool =>
    true

  fun ref next(): A =>
    _v
