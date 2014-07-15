use "../collections" as c

trait Foo

trait Bar

type FooBar is (Foo | Bar)

class FooBarTest[A: (Foo | Bar)]

class AnyTest[A: Any]

class StructTest[A: {fun tag call_it({fun ref(I64): I64}): I64}]

class Wombat
  var x: I64

  new create() => x = 0
  /*fun box fail() => x = 1*/
  fun ref pass() => x = 2

class Precedence is Foo, Bar
  /* nested
  /* comments */
  work */
  var field: I32
  var foo_bar_test: FooBarTest[FooBar]
  var any_test: AnyTest[String]
  var struct_test: StructTest[Precedence]

  fun tag wombat[A: Arithmetic, B: Integer = I32](a: A, b: B): A => a + a

  fun tag find_in_array(a: c.Array[String] val, find: String):
    (U64, String) ? =>
    wombat[F32](1.5, 2)
    for (index, value) in a.pairs() do
      if value == find then
        return (index, value)
      end
    end
    error

  fun tag wrap_find(a: c.Array[String] val, find: String): (U64, String) =>
    try
      find_in_array(a, find)
    else
      (0, "Not found")
    end

  fun tag call_it(a: {fun ref(I64): I64}): I64 => a(1)

  /*fun tag make_it(i: I64): {fun ref(I64): I64} =>
    {
      var i' = i
      fun ref apply(j: I64): I64 => (i' = j) + j
    }*/

  fun tag make_array(n: U64): c.Array[U64] iso^ =>
    var a = recover c.Array[U64]
    a.reserve(n)
    for i in c.Range[U64](0, n) do
      a.append(i)
    end
    consume a
    /*consume [1, 2, 3, n]*/

  fun tag make_iso_array(n: U64): c.Array[U64] iso^ =>
    recover make_array(n)

  fun tag optionals(a: I64 = 1, b: I64 = a): I64 => a * b

  /*fun tag call_optionals(): I64 => optionals(3 where b = 10)*/

  /** This is doc /**/
   * @param foo blah /* bar */
   * [[http://foo]]
   * @return something // foo
   * foo /* foo */
   */
  /* FIXME: foo */
  // foo
  fun val foo(): F32 =>
    var x: F32 = 3
    3 == x
    1 + 2 * 3 + 4
    """
      blargh
    test /* go nuts */
    this is a test
    foo
    //"""

    """     something is \n here"""
    "           something is \n here"
    "           something is
    here"
    /*-*/3

  fun tag identity[A](a: A): A^ => consume a
