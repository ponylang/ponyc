use "pony_test"

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  fun tag tests(test: PonyTest) =>
    JSONTests.tests(test)
    JSONPathTests.tests(test)
    JSONBuilderTests.tests(test)
