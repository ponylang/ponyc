use "pony_test"
use mutate = ".."

class \nodoc\ _TestInfoFileFilterNoFilters is UnitTest
  fun name(): String =>
    "InfoFile.filter: no filters returns all mutants"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result = mutate.InfoFile.filter(mutants)
    h.assert_eq[USize](4, result.size())

class \nodoc\ _TestInfoFileFilterByOperator is UnitTest
  fun name(): String =>
    "InfoFile.filter: operator filter keeps matching categories"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(mutants where operators = "aor")
    h.assert_eq[USize](1, result.size())
    try
      h.assert_eq[String]("aor", result(0)?.category)
    else
      h.fail("index out of bounds")
    end

class \nodoc\ _TestInfoFileFilterByMultipleOperators is UnitTest
  fun name(): String =>
    "InfoFile.filter: comma-separated operators"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(
        mutants where operators = "aor,ror")
    h.assert_eq[USize](2, result.size())

class \nodoc\ _TestInfoFileFilterByOperatorWithSpaces is UnitTest
  fun name(): String =>
    "InfoFile.filter: spaces around operator names are trimmed"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(
        mutants where operators = " aor , ror ")
    h.assert_eq[USize](2, result.size())

class \nodoc\ _TestInfoFileFilterByScope is UnitTest
  fun name(): String =>
    "InfoFile.filter: scope filter keeps matching files"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(
        mutants where scope = "src/main.pony")
    h.assert_eq[USize](3, result.size())

class \nodoc\ _TestInfoFileFilterByScopePartialPath is UnitTest
  fun name(): String =>
    "InfoFile.filter: scope matches path components, not substrings"

  fun apply(h: TestHelper) =>
    let mutants =
      recover val
        [ as mutate.Mutant:
          mutate.Mutant(
            1, 0, "src/main.pony", 1, 1, "aor", "+", "-")
          mutate.Mutant(
            2,
            0,
            "src/main_helper.pony",
            1,
            1,
            "aor",
            "+",
            "-")]
      end
    let result =
      mutate.InfoFile.filter(
        mutants where scope = "main.pony")
    h.assert_eq[USize](1, result.size())
    try
      h.assert_eq[String](
        "src/main.pony", result(0)?.file)
    else
      h.fail("index out of bounds")
    end

class \nodoc\ _TestInfoFileFilterByScopeDirectory is UnitTest
  fun name(): String =>
    "InfoFile.filter: scope matches a directory component"

  fun apply(h: TestHelper) =>
    let mutants =
      recover val
        [ as mutate.Mutant:
          mutate.Mutant(
            1, 0, "src/main.pony", 1, 1, "aor", "+", "-")
          mutate.Mutant(
            2, 0, "lib/util.pony", 1, 1, "aor", "+", "-")]
      end
    let result =
      mutate.InfoFile.filter(mutants where scope = "src")
    h.assert_eq[USize](1, result.size())
    try
      h.assert_eq[String](
        "src/main.pony", result(0)?.file)
    else
      h.fail("index out of bounds")
    end

class \nodoc\ _TestInfoFileFilterCombined is UnitTest
  fun name(): String =>
    "InfoFile.filter: operator and scope together"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(
        mutants
          where operators = "aor", scope = "src/main.pony")
    h.assert_eq[USize](1, result.size())
    try
      h.assert_eq[I32](1, result(0)?.id)
    else
      h.fail("index out of bounds")
    end

class \nodoc\ _TestInfoFileFilterNoMatch is UnitTest
  fun name(): String =>
    "InfoFile.filter: no matches returns empty array"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(mutants where operators = "uoi")
    h.assert_eq[USize](0, result.size())

class \nodoc\ _TestInfoFileFilterEmptyInput is UnitTest
  fun name(): String =>
    "InfoFile.filter: empty mutant array returns empty"

  fun apply(h: TestHelper) =>
    let mutants: Array[mutate.Mutant] val =
      recover val Array[mutate.Mutant] end
    let result =
      mutate.InfoFile.filter(mutants where operators = "aor")
    h.assert_eq[USize](0, result.size())

class \nodoc\ _TestInfoFileFilterScopeAtStart is UnitTest
  fun name(): String =>
    "InfoFile.filter: scope matching at start of path"

  fun apply(h: TestHelper) =>
    let mutants =
      recover val
        [ as mutate.Mutant:
          mutate.Mutant(
            1, 0, "main.pony", 1, 1, "aor", "+", "-")]
      end
    let result =
      mutate.InfoFile.filter(
        mutants where scope = "main.pony")
    h.assert_eq[USize](1, result.size())

class \nodoc\ _TestInfoFileFilterScopeBackslash is UnitTest
  fun name(): String =>
    "InfoFile.filter: scope matches with backslash separators"

  fun apply(h: TestHelper) =>
    let mutants =
      recover val
        [ as mutate.Mutant:
          mutate.Mutant(
            1,
            0,
            "src\\main.pony",
            1,
            1,
            "aor",
            "+",
            "-")]
      end
    let result =
      mutate.InfoFile.filter(
        mutants where scope = "main.pony")
    h.assert_eq[USize](1, result.size())

class \nodoc\ _TestInfoFileFilterScopeEmpty is UnitTest
  fun name(): String =>
    "InfoFile.filter: empty scope string acts as no filter"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(mutants where scope = "")
    h.assert_eq[USize](4, result.size())

class \nodoc\ _TestInfoFileFilterOperatorEmpty is UnitTest
  fun name(): String =>
    "InfoFile.filter: empty operator string acts as no filter"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(mutants where operators = "")
    h.assert_eq[USize](4, result.size())

class \nodoc\ _TestInfoFileFilterByAmr is UnitTest
  fun name(): String =>
    "InfoFile.filter: amr operator filter keeps AMR mutants"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(mutants where operators = "amr")
    h.assert_eq[USize](1, result.size())
    try
      h.assert_eq[String]("amr", result(0)?.category)
      h.assert_eq[String]("add", result(0)?.original)
      h.assert_eq[String]("add_unsafe", result(0)?.replacement)
    else
      h.fail("index out of bounds")
    end

class \nodoc\ _TestInfoFileFilterByScopeLine is UnitTest
  fun name(): String =>
    "InfoFile.filter: scope with single line"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(
        mutants where scope = "src/main.pony:5")
    h.assert_eq[USize](1, result.size())
    try
      h.assert_eq[I32](2, result(0)?.id)
    else
      h.fail("index out of bounds")
    end

class \nodoc\ _TestInfoFileFilterByScopeLineRange is UnitTest
  fun name(): String =>
    "InfoFile.filter: scope with line range"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(
        mutants where scope = "src/main.pony:1-3")
    h.assert_eq[USize](2, result.size())

class \nodoc\ _TestInfoFileFilterByScopeLineRangeNoMatch is UnitTest
  fun name(): String =>
    "InfoFile.filter: scope line range excludes out-of-range mutants"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(
        mutants where scope = "src/main.pony:10-20")
    h.assert_eq[USize](0, result.size())

class \nodoc\ _TestInfoFileFilterByScopeLineWithOperator is UnitTest
  fun name(): String =>
    "InfoFile.filter: scope line range combined with operator"

  fun apply(h: TestHelper) =>
    let mutants = _FourMutants()
    let result =
      mutate.InfoFile.filter(
        mutants where operators = "aor", scope = "src/main.pony:1-5")
    h.assert_eq[USize](1, result.size())
    try
      h.assert_eq[I32](1, result(0)?.id)
    else
      h.fail("index out of bounds")
    end

primitive \nodoc\ _FourMutants
  fun apply(): Array[mutate.Mutant] val =>
    recover val
      [ as mutate.Mutant:
        mutate.Mutant(
          1, 0, "src/main.pony", 1, 1, "aor", "+", "-")
        mutate.Mutant(
          2, 0, "src/main.pony", 5, 1, "ror", "==", "!=")
        mutate.Mutant(
          3, 0, "lib/util.pony", 2, 1, "bor", "and", "or")
        mutate.Mutant(
          4, 0, "src/main.pony", 1, 1, "amr", "add", "add_unsafe")]
    end
