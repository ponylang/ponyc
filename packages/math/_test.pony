use "pony_test"
use "random"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_IsPrimeTestBuilder[U8]("U8"))
    test(_IsPrimeTestBuilder[U16]("U16"))
    test(_IsPrimeTestBuilder[U32]("U32"))
    test(_IsPrimeTestBuilder[U64]("U64"))
    test(_IsPrimeTestBuilder[U128]("U128"))
    test(_IsPrimeTestBuilder[ULong]("ULong"))
    test(_IsPrimeTestBuilder[USize]("USize"))
    test(_MersennePrimeTest[U8]("U8"))
    test(_MersennePrimeTest[U16]("U16"))
    test(_MersennePrimeTest[U32]("U32"))
    test(_MersennePrimeTest[U64]("U64"))
    test(_MersennePrimeTest[ULong]("ULong"))
    test(_MersennePrimeTest[USize]("USize"))
    test(_TestGreatestCommonDivisor)
    test(_TestGreatestCommonDivisorErrorsOnZero)
    test(_TestLeastCommonMultiple)
    test(_TestLeastCommonMultipleErrorsOnOverflow)
    test(_TestLeastCommonMultipleErrorsOnZero)

primitive \nodoc\ _IsPrimeTestBuilder[A: (UnsignedInteger[A] val & Unsigned)]
  """
  All integers (excluding 2 and 3), can be expressed as (6k + i),
  where i = −1, 0, 1, 2, 3, or 4 and k is greater than 0.
  Since 2 divides (6k + 0), (6k + 2),
  and (6k + 4), while 3 divides (6k + 3) that leaves only (6k - 1) and (6k + 1)
  for expressing primes.

  Given the above, (6k + 0), (6k + 2), 6k + 4), (6k + 3) should always express composites.
  """
  fun apply(s: String): UnitTest iso^ =>
    object iso is UnitTest
      fun name(): String => "math/IsPrime -- " + s

      fun apply(h: TestHelper) =>
        let rand = Rand
        let max_checkable = (A.max_value() - 4) / 6
        let n: USize = (A(0).bytewidth() * 8) * 2
        var i: USize = 0
        while i < n do
          i = i + 1
          let k: A = rand.int[A](max_checkable)
          if k == 0 then
            continue
          end
          let plusNone: A = 6 * k
          let plusTwo: A = plusNone + 2
          let plusThree: A = plusNone + 3
          let plusFour: A = plusNone + 4

          h.assert_false(IsPrime[A](plusNone))
          h.assert_false(IsPrime[A](plusTwo))
          h.assert_false(IsPrime[A](plusThree))
          h.assert_false(IsPrime[A](plusFour))
        end

        h.assert_false(IsPrime[A](0))
        h.assert_false(IsPrime[A](1))
    end

primitive \nodoc\ _MersennePrimeTest[A: (UnsignedInteger[A] val & Unsigned)]
  """
  A Mersenne prime is a prime of the form (2 ^ p) - 1 where p is itself prime
  """
  fun apply(s: String): UnitTest iso^ =>
    object iso is UnitTest
      fun name(): String => "math/MersennePrimeTest -- " + s

      fun apply(h: TestHelper) =>
        let mersenne_prime_exponents: Array[A] = [
          2; 3; 5; 7
          13; 17; 19; 31
          61; 89; 107; 127
        ]
        var p: A = A(0).bitwidth()
        var x: A = A.max_value()  // (2 ^ p) - 1
        while p >= 2 do
          let isprime = IsPrime[A](x)
          if mersenne_prime_exponents.contains(p) then
            h.assert_true(isprime)
          else
            h.assert_false(isprime)
          end
          p = p - 1
          x = x >> 1
        end
    end

class \nodoc\ iso _TestGreatestCommonDivisorErrorsOnZero is UnitTest
  """
  Verify that giving a zero to GreatestCommonDivisor will result in an error.
  """
  fun name(): String => "math/GreatestCommonDivisorErrorsOnZero"

  fun apply(h: TestHelper) =>
    h.assert_error({() ? => GreatestCommonDivisor[U8](0,1)? })
    h.assert_error({() ? => GreatestCommonDivisor[U8](1,0)? })
    h.assert_error({() ? => GreatestCommonDivisor[U8](0,0)? })

class \nodoc\ iso _TestGreatestCommonDivisor is UnitTest
  """
  A few tests to verify we get a value that we expect. This is far from being an
  exhaustive test.
  """
  fun name(): String => "math/GreatestCommonDivisor"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[U64](1, GreatestCommonDivisor[U64](1,10)?)
    h.assert_eq[U64](1, GreatestCommonDivisor[U64](10,1)?)
    h.assert_eq[U64](1, GreatestCommonDivisor[U64](7,53)?)
    h.assert_eq[U64](1, GreatestCommonDivisor[U64](53,7)?)
    h.assert_eq[U64](4, GreatestCommonDivisor[U64](4,20)?)
    h.assert_eq[U64](4, GreatestCommonDivisor[U64](20,4)?)
    h.assert_eq[U64](5, GreatestCommonDivisor[U64](25,20)?)
    h.assert_eq[U64](5, GreatestCommonDivisor[U64](20,25)?)

class \nodoc\ iso _TestLeastCommonMultipleErrorsOnZero is UnitTest
  """
  Verify that giving a zero to LeastCommonMultiple will result in an error.
  """
  fun name(): String => "math/LeastCommonMultipleErrorsOnZero"

  fun apply(h: TestHelper) =>
    h.assert_error({() ? => LeastCommonMultiple[U8](0,1)? })
    h.assert_error({() ? => LeastCommonMultiple[U8](1,0)? })
    h.assert_error({() ? => LeastCommonMultiple[U8](0,0)? })

class \nodoc\ iso _TestLeastCommonMultipleErrorsOnOverflow is UnitTest
  """
  Verify creating an lcm that overflows results in an error
  """
  fun name(): String => "math/LeastCommonMultipleErrorsOnOverflow"

  fun apply(h: TestHelper) =>
    h.assert_error({() ? => LeastCommonMultiple[U8](200,205)? })
    h.assert_error({() ? => LeastCommonMultiple[U8](205,200)? })

class \nodoc\ iso _TestLeastCommonMultiple is UnitTest
  """
  A few tests to verify we get a value that we expect. This is far from being an
  exhaustive test.
  """
  fun name(): String => "math/LeastCommonMultiple"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[U64](8200, LeastCommonMultiple[U64](200,205)?)
    h.assert_eq[U64](8200, LeastCommonMultiple[U64](205,200)?)
    h.assert_eq[U64](1683, LeastCommonMultiple[U64](17,99)?)
    h.assert_eq[U64](1683, LeastCommonMultiple[U64](99,17)?)
    h.assert_eq[U64](20, LeastCommonMultiple[U64](4,20)?)
    h.assert_eq[U64](20, LeastCommonMultiple[U64](20,4)?)
    h.assert_eq[U64](100, LeastCommonMultiple[U64](25,20)?)
    h.assert_eq[U64](100, LeastCommonMultiple[U64](20,25)?)
