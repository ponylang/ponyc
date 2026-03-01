use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestFailureMultipleMessages)
    test(_TestFailurePlumbingWorks)
    test(_TestMultipleMessagesSanity)
    test(_TestSuccessPlumbingWorks)

class \nodoc\ iso _TestSuccessPlumbingWorks is UnitTest
  """
  Test that what should be a success, comes back as a success.
  We are expecting to get a constrained type back.
  """
  fun name(): String => "constrained_types/SuccessPlumbingWorks"

  fun ref apply(h: TestHelper) =>
    let less: USize = 9

    match \exhaustive\ MakeConstrained[USize, _LessThan10Validator](less)
    | let s: Constrained[USize, _LessThan10Validator] =>
      h.assert_true(true)
    | let f: ValidationFailure =>
      h.assert_true(false)
    end

class \nodoc\ iso _TestFailurePlumbingWorks is UnitTest
  """
  Test that what should be a failure, comes back as a failure.
  This is a basic plumbing test.
  """
  fun name(): String => "constrained_types/FailurePlumbingWorks"

  fun ref apply(h: TestHelper) =>
    let more: USize = 11

    match \exhaustive\ MakeConstrained[USize, _LessThan10Validator](more)
    | let s: Constrained[USize, _LessThan10Validator] =>
      h.assert_true(false)
    | let f: ValidationFailure =>
      h.assert_true(f.errors().size() == 1)
      h.assert_array_eq[String](["not less than 10"], f.errors())
    end

class \nodoc\ iso _TestMultipleMessagesSanity is UnitTest
  """
  Sanity check that the _MultipleErrorsValidator works as expected and that
  we can trust the _TestFailureMultipleMessages working results.
  """
  fun name(): String => "constrained_types/MultipleMessagesSanity"

  fun ref apply(h: TestHelper) =>
    let string = "magenta"

    match \exhaustive\ MakeConstrained[String, _MultipleErrorsValidator](string)
    | let s: Constrained[String, _MultipleErrorsValidator] =>
      h.assert_true(true)
    | let f: ValidationFailure =>
      h.assert_true(false)
    end

class \nodoc\ iso _TestFailureMultipleMessages is UnitTest
  """
  Verify that collecting errors works as expected.
  """
  fun name(): String => "constrained_types/FailureMultipleMessages"

  fun ref apply(h: TestHelper) =>
    let string = "A1"

    match \exhaustive\ MakeConstrained[String, _MultipleErrorsValidator](string)
    | let s: Constrained[String, _MultipleErrorsValidator] =>
      h.assert_true(false)
    | let f: ValidationFailure =>
      h.assert_true(f.errors().size() == 2)
      h.assert_array_eq_unordered[String](
        ["bad length"; "bad character"],
        f.errors())
    end

primitive \nodoc\ _LessThan10Validator is Validator[USize]
  fun apply(num: USize): ValidationResult =>
    recover val
      if num < 10 then
        ValidationSuccess
      else
        ValidationFailure("not less than 10")
      end
    end

primitive \nodoc\ _MultipleErrorsValidator is Validator[String]
  fun apply(string: String): ValidationResult =>
    recover val
      let errors: Array[String] = Array[String]()

      // Isn't too big or too small
      if (string.size() < 6) or (string.size() > 12) then
        let msg = "bad length"
        errors.push(msg)
      end

      // Every character is valid
      for c in string.values() do
        if (c < 97) or (c > 122) then
          errors.push("bad character")
          break
        end
      end

      if errors.size() == 0 then
        ValidationSuccess
      else
        // We have some errors, let's package them all up
        // and return the failure
        let failure = ValidationFailure
        for e in errors.values() do
          failure(e)
        end
        failure
      end
    end
