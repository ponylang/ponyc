use "pony_test"
use lsp = "pony-lsp/test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    lsp.Main.make().tests(test)
