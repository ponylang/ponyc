use "../collections" as c

class Precedence
  /* nested
  /* comments */
  work */

  /**/
  fun mut test() =>
    for i in j do
      x
    else
      y
    end

  /** This is doc /**/
   * @param foo blah /* bar */
   * [[http://foo]]
   * @return something // foo
   */
  // foo
  fun imm foo(): F32 =>
    1 + 2 * 3 + 4
    -3
    """
      blargh
      test
        this is a test
          foo
          """

    """     something is \n here"""
    "           something is \n here"

  fun tag identity[A](a: A): A^ => consume a

  fun mut bar(): I32 =>
    3.hello
    [A](1, 2)
    foo.bar[Z](99, 100)
    (7; 3,77)
    a + 37; b + b * b
