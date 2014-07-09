use "../collections" as c

trait Foo

trait Bar

type FooBar is (Foo | Bar)

class FooBarTest[A: (Foo | Bar)]

class AnyTest[A: Any]

class StructTest[A: {fun tag call_it({fun ref(I64): I64}): I64}]

class Precedence is Foo, Bar
  /* nested
  /* comments */
  work */
  var field: I32
  var foo_bar_test: FooBarTest[FooBar]
  var any_test: AnyTest[String]
  var struct_test: StructTest[Precedence]

  fun tag find_in_array(a: c.Array[String] val, find: String):
    (U64, String) ? =>
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

  fun tag call_it(a: {fun ref(I64): I64}): I64 => a()

  /*fun tag make_it(i: I64): {fun ref(I64): I64} =>
    {
      var i' = i
      fun ref apply(j: I64): I64 => (i' = j) + j
    }*/

  fun tag make_array(n: U64): c.Array[U64] ref^ =>
    var a = c.Array[U64]
    a.reserve(n)
    for i in c.Range(0, n) do
      a.append(i)
    end
    consume a
    /*consume [1, 2, 3, n]*/

  fun tag make_iso_array(n: U64): c.Array[U64] iso^ =>
    recover make_array(n)

  fun tag optionals(a: I64 = 1, b: I64 = 1): I64 => a * b

  /*fun tag call_optionals(): I64 => optionals(3 where b = 10)*/

  /**/
  fun ref test() =>
    var list
    for (i, j) in list do
      1
    else
      2
    end

  /** This is doc /**/
   * @param foo blah /* bar */
   * [[http://foo]]
   * @return something // foo
   * TODO: foo /* TODO: foo */
   */
  /* FIXME: foo */
  // TODO: foo
  fun val foo(): F32 =>
    1 + 2 * 3 + 4
    -3
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

  fun tag identity[A](a: A): A^ => consume a

  fun ref bar(): I32 =>
    3.hello
    c.List(9)
    [True](1, 2)
    "string".bar[c.List[Precedence]](99, 100)
    (7; 3,77)
    var (a, b)
    a + 37; b + b * b
