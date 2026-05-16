use "pony_test"
use ".."

primitive _SymbolKindsTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_SymbolKindsArrayTest)

class \nodoc\ iso _SymbolKindsArrayTest is UnitTest
  fun name(): String => "symbol_kinds/array"

  fun apply(h: TestHelper) =>
    h.assert_eq[I64](18, SymbolKinds.array())
