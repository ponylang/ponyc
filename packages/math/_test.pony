use "ponytest"
use "random"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_IsPrimeTestBuilder[U8].construct("U8"))
    test(_IsPrimeTestBuilder[U16].construct("U16"))
    test(_IsPrimeTestBuilder[U32].construct("U32"))
    test(_IsPrimeTestBuilder[U64].construct("U64"))
    test(_IsPrimeTestBuilder[U128].construct("U128"))

primitive _IsPrimeTestBuilder[A: (Integer[A] val & Unsigned)]
  fun construct(s: String): UnitTest iso^ =>
    object iso is UnitTest
      fun name(): String => "math/IsPrime -- " + s 

      fun apply(h: TestHelper) =>
        // All integers (excluding 2 and 3), can be expressed as (6k + i), 
        // where i = −1, 0, 1, 2, 3, or 4. Since 2 divides (6k + 0), (6k + 2), 
        // and (6k + 4), while 3 divides (6k + 3) that leaves only (6k - 1) and (6k + 1) 
        // for expressing primes.
        //
        // Given the above, (6k + 0), (6k + 2), 6k + 4), (6k + 3) should always express composites.
        let max_checkable = (A.max_value() - 4) / 6
        let k: A = Rand.int[A](max_checkable)
        let plusNone: A = 6 * k
        let plusTwo: A = plusNone + 2
        let plusThree: A = plusNone + 3
        let plusFour: A = plusNone + 4  

        h.assert_false(IsPrime[A](plusNone))
        h.assert_false(IsPrime[A](plusTwo))
        h.assert_false(IsPrime[A](plusThree))
        h.assert_false(IsPrime[A](plusFour))
    end