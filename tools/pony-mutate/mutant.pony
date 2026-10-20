class val Mutant
  """
  A single mutant record from the info file.
  """
  let id: I32
  let file: String val
  let line: USize
  let column: USize
  let category: String val
  let original: String val
  let replacement: String val

  new val create(
    id': I32,
    file': String val,
    line': USize,
    column': USize,
    category': String val,
    original': String val,
    replacement': String val)
  =>
    id = id'
    file = file'
    line = line'
    column = column'
    category = category'
    original = original'
    replacement = replacement'

  fun location(): String val =>
    file + ":" + line.string() + ":" + column.string()

  fun description(): String val =>
    category + " " + original + " -> " + replacement

primitive MutantKilled
  """
  The test suite detected the mutation (non-zero exit).
  """

primitive MutantSurvived
  """
  The test suite passed with the mutation active.
  """

primitive MutantTimedOut
  """
  The mutant run exceeded the timeout.
  """

type MutantResult is (MutantKilled | MutantSurvived | MutantTimedOut)

class val MutantOutcome is Comparable[MutantOutcome]
  """
  Result of running a single mutant.
  """
  let mutant: Mutant
  let result: MutantResult
  let duration_ns: U64

  new val create(
    mutant': Mutant,
    result': MutantResult,
    duration_ns': U64)
  =>
    mutant = mutant'
    result = result'
    duration_ns = duration_ns'

  fun lt(that: MutantOutcome box): Bool =>
    if mutant.file != that.mutant.file then
      return mutant.file < that.mutant.file
    end
    if mutant.line != that.mutant.line then
      return mutant.line < that.mutant.line
    end
    if mutant.column != that.mutant.column then
      return mutant.column < that.mutant.column
    end
    mutant.id < that.mutant.id

  fun eq(that: MutantOutcome box): Bool =>
    mutant.id == that.mutant.id
