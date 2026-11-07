use "pony_test"
use mutate = ".."

class \nodoc\ _TestMutantLocation is UnitTest
  fun name(): String => "Mutant: location() formats file:line:column"

  fun apply(h: TestHelper) =>
    let m =
      mutate.Mutant(1, 0, "src/main.pony", 10, 5, "aor", "+", "-")
    h.assert_eq[String]("src/main.pony:10:5", m.location())

class \nodoc\ _TestMutantDescription is UnitTest
  fun name(): String =>
    "Mutant: description() formats category original -> replacement"

  fun apply(h: TestHelper) =>
    let m =
      mutate.Mutant(1, 0, "f.pony", 1, 1, "ror", "==", "!=")
    h.assert_eq[String]("ror == -> !=", m.description())

class \nodoc\ _TestMutantOutcomeLtByFile is UnitTest
  fun name(): String => "MutantOutcome: sorts by file first"

  fun apply(h: TestHelper) =>
    let a =
      _outcome("a.pony", 100, 1, 1, mutate.MutantKilled)
    let b =
      _outcome("b.pony", 1, 1, 2, mutate.MutantKilled)
    h.assert_true(a < b)
    h.assert_false(b < a)

  fun _outcome(
    file: String val,
    line: USize,
    col: USize,
    id: I32,
    result: mutate.MutantResult)
    : mutate.MutantOutcome
  =>
    mutate.MutantOutcome(
      mutate.Mutant(
        id, 0, file, line, col, "aor", "+", "-"),
      result,
      0)

class \nodoc\ _TestMutantOutcomeLtByLine is UnitTest
  fun name(): String =>
    "MutantOutcome: sorts by line within same file"

  fun apply(h: TestHelper) =>
    let a = _outcome(5, 80, 1)
    let b = _outcome(10, 1, 2)
    h.assert_true(a < b)
    h.assert_false(b < a)

  fun _outcome(
    line: USize,
    col: USize,
    id: I32)
    : mutate.MutantOutcome
  =>
    mutate.MutantOutcome(
      mutate.Mutant(
        id, 0, "f.pony", line, col, "aor", "+", "-"),
      mutate.MutantKilled,
      0)

class \nodoc\ _TestMutantOutcomeLtByColumn is UnitTest
  fun name(): String =>
    "MutantOutcome: sorts by column within same file and line"

  fun apply(h: TestHelper) =>
    let a = _outcome(3, 1)
    let b = _outcome(10, 2)
    h.assert_true(a < b)
    h.assert_false(b < a)

  fun _outcome(col: USize, id: I32)
    : mutate.MutantOutcome
  =>
    mutate.MutantOutcome(
      mutate.Mutant(
        id, 0, "f.pony", 5, col, "aor", "+", "-"),
      mutate.MutantKilled,
      0)

class \nodoc\ _TestMutantOutcomeLtById is UnitTest
  fun name(): String =>
    "MutantOutcome: sorts by id when file, line, column match"

  fun apply(h: TestHelper) =>
    let m1 =
      mutate.Mutant(1, 0, "f.pony", 5, 3, "aor", "+", "-")
    let m2 =
      mutate.Mutant(2, 0, "f.pony", 5, 3, "aor", "+", "*")
    let a = mutate.MutantOutcome(m1, mutate.MutantKilled, 0)
    let b = mutate.MutantOutcome(m2, mutate.MutantKilled, 0)
    h.assert_true(a < b)
    h.assert_false(b < a)

class \nodoc\ _TestMutantOutcomeEq is UnitTest
  fun name(): String => "MutantOutcome: equality by mutant id"

  fun apply(h: TestHelper) =>
    let m1 =
      mutate.Mutant(7, 0, "a.pony", 1, 1, "aor", "+", "-")
    let m2 =
      mutate.Mutant(
        7, 0, "b.pony", 99, 99, "ror", "==", "!=")
    let a =
      mutate.MutantOutcome(m1, mutate.MutantKilled, 100)
    let b =
      mutate.MutantOutcome(m2, mutate.MutantSurvived, 200)
    h.assert_true(a == b)

class \nodoc\ _TestMutantOutcomeNotEq is UnitTest
  fun name(): String =>
    "MutantOutcome: different ids are not equal"

  fun apply(h: TestHelper) =>
    let m1 =
      mutate.Mutant(1, 0, "f.pony", 1, 1, "aor", "+", "-")
    let m2 =
      mutate.Mutant(2, 0, "f.pony", 1, 1, "aor", "+", "-")
    let a = mutate.MutantOutcome(m1, mutate.MutantKilled, 0)
    let b = mutate.MutantOutcome(m2, mutate.MutantKilled, 0)
    h.assert_false(a == b)
