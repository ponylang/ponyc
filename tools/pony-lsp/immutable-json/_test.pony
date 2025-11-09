use "pony_test"

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    _JSONTests.tests(test)
    _JSONPathTests.tests(test)
    _JSONBuilderTests.tests(test)
