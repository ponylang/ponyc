class Zip2[A, B] is Iterator[(A, B)]
  """
  Zip two iterators together so that each call to next() results in
  a tuple with the next value of the first iterator and the next value
  of the second iterator. The number of items returned is the minimum of
  the number of items returned by the two iterators.

  ## Example program

  Print the following:

  ```
  1 a
  2 b
  3 c
  ```

  ```pony
  use "itertools"

  actor Main
    new create(env: Env) =>
      var i1: Array[I32] ref = [as I32: 1; 2; 3]
      var i2: Array[String] ref = ["a"; "b"; "c"; "d"]

      for (x, y) in Zip2[I32, String](i1.values(), i2.values()) do
        env.out.print("".add(x.string()).add(" ").add(y))
      end
  ```
  """
  let _i1: Iterator[A]
  let _i2: Iterator[B]

  new create(i1: Iterator[A] ref, i2: Iterator[B] ref) =>
    _i1 = consume i1
    _i2 = consume i2

  fun ref has_next(): Bool =>
    _i1.has_next() and _i2.has_next()

  fun ref next(): (A, B) ? =>
    (_i1.next()?, _i2.next()?)

class Zip3[A, B, C] is Iterator[(A, B, C)]
  """
  Zip three iterators together so that each call to next() results in
  a tuple with the next value of the first iterator, the next value
  of the second iterator, and the value of the third iterator. The
  number of items returned is the minimum of the number of items
  returned by the three iterators.

  ## Example Program

  Print the following:

  ```
  1 a 0.5
  2 b 9.8
  3 c 3.3
  ```

  ```pony
  use "itertools"

  actor Main
    new create(env: Env) =>
      var i1: Array[I32] ref = [as I32: 1; 2; 3]
      var i2: Array[String] ref = ["a"; "b"; "c"; "d"]
      var i3: Array[F32] ref = [as F32: 0.5; 9.8; 3.3]

      for (x, y, z) in Zip3[I32, String, F32](
        i1.values(), i2.values(), i3.values())
      do
        env.out.print(""
          .add(x.string()).add(" ")
          .add(y).add(" ")
          .add(z.string()))
      end
  ```
  """
  let _i1: Iterator[A]
  let _i2: Iterator[B]
  let _i3: Iterator[C]

  new create(i1: Iterator[A] ref, i2: Iterator[B] ref, i3: Iterator[C]) =>
    _i1 = i1
    _i2 = i2
    _i3 = i3

  fun ref has_next(): Bool =>
    _i1.has_next() and _i2.has_next() and _i3.has_next()

  fun ref next(): (A, B, C) ? =>
    (_i1.next()?, _i2.next()?, _i3.next()?)

class Zip4[A, B, C, D] is Iterator[(A, B, C, D)]
  """
  Zip four iterators together so that each call to next() results in
  a tuple with the next value of each of the iterators. The number of
  items returned is the minimum of the number of items returned by the
  iterators.

  ## Example Program

  Print the following:

  ```
  1 a 0.5 m
  2 b 9.8 n
  3 c 3.3 o
  ```

  ```pony
  use "itertools"

  actor Main
    new create(env: Env) =>
      var i1: Array[I32] ref = [as I32: 1; 2; 3]
      var i2: Array[String] ref = ["a"; "b"; "c"; "d"]
      var i3: Array[F32] ref = [as F32: 0.5; 9.8; 3.3]
      var i4: Array[String] ref = ["m"; "n"; "o"; "p"; "q"; "r"]

      for (x, y, z, xx) in Zip4[I32, String, F32, String](
        i1.values(), i2.values(), i3.values(), i4.values())
      do
        env.out.print(""
          .add(x.string()).add(" ")
          .add(y).add(" ")
          .add(z.string()).add(" ")
          .add(xx.string()))
      end
  ```
  """
  let _i1: Iterator[A]
  let _i2: Iterator[B]
  let _i3: Iterator[C]
  let _i4: Iterator[D]

  new create(
    i1: Iterator[A],
    i2: Iterator[B],
    i3: Iterator[C],
    i4: Iterator[D])
  =>
    _i1 = i1
    _i2 = i2
    _i3 = i3
    _i4 = i4

  fun ref has_next(): Bool =>
    _i1.has_next() and _i2.has_next() and _i3.has_next() and _i4.has_next()

  fun ref next(): (A, B, C, D) ? =>
    (_i1.next()?, _i2.next()?, _i3.next()?, _i4.next()?)

class Zip5[A, B, C, D, E] is Iterator[(A, B, C, D, E)]
  """
  Zip five iterators together so that each call to next() results in
  a tuple with the next value of each of the iterators. The number of
  items returned is the minimum of the number of items returned by the
  iterators.

  ## Example Program

  Print the following:

  ```
  1 a 0.5 m 2
  2 b 9.8 n 6
  3 c 3.3 o 5
  ```

  ```pony
  use "itertools"

  actor Main
    new create(env: Env) =>
      var i1: Array[I32] ref = [as I32: 1; 2; 3]
      var i2: Array[String] ref = ["a"; "b"; "c"; "d"]
      var i3: Array[F32] ref = [as F32: 0.5; 9.8; 3.3]
      var i4: Array[String] ref = ["m"; "n"; "o"; "p"; "q"; "r"]
      var i5: Array[I32] ref = [as I32: 2; 6; 5; 3; 5]

      for (x, y, z, xx, yy) in Zip5[I32, String, F32, String, I32](
        i1.values(), i2.values(), i3.values(), i4.values(), i5.values())
      do
        env.out.print(""
          .add(x.string()).add(" ")
          .add(y).add(" ")
          .add(z.string()).add(" ")
          .add(xx.string()).add(" ")
          .add(yy.string()))
      end
  ```
  """
  let _i1: Iterator[A]
  let _i2: Iterator[B]
  let _i3: Iterator[C]
  let _i4: Iterator[D]
  let _i5: Iterator[E]

  new create(
    i1: Iterator[A],
    i2: Iterator[B],
    i3: Iterator[C],
    i4: Iterator[D],
    i5: Iterator[E])
  =>
    _i1 = i1
    _i2 = i2
    _i3 = i3
    _i4 = i4
    _i5 = i5

  fun ref has_next(): Bool =>
    _i1.has_next()
      and _i2.has_next()
      and _i3.has_next()
      and _i4.has_next()
      and _i5.has_next()

  fun ref next(): (A, B, C, D, E) ? =>
    (_i1.next()?, _i2.next()?, _i3.next()?, _i4.next()?, _i5.next()?)
