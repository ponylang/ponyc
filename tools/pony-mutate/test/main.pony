use "pony_test"
use mutate = ".."

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Mutant
    test(_TestMutantLocation)
    test(_TestMutantDescription)

    // MutantOutcome ordering
    test(_TestMutantOutcomeLtByFile)
    test(_TestMutantOutcomeLtByLine)
    test(_TestMutantOutcomeLtByColumn)
    test(_TestMutantOutcomeLtById)
    test(_TestMutantOutcomeEq)
    test(_TestMutantOutcomeNotEq)

    // InfoFile.filter
    test(_TestInfoFileFilterNoFilters)
    test(_TestInfoFileFilterByOperator)
    test(_TestInfoFileFilterByMultipleOperators)
    test(_TestInfoFileFilterByOperatorWithSpaces)
    test(_TestInfoFileFilterByScope)
    test(_TestInfoFileFilterByScopePartialPath)
    test(_TestInfoFileFilterByScopeDirectory)
    test(_TestInfoFileFilterCombined)
    test(_TestInfoFileFilterNoMatch)
    test(_TestInfoFileFilterEmptyInput)
    test(_TestInfoFileFilterScopeAtStart)
    test(_TestInfoFileFilterScopeBackslash)
    test(_TestInfoFileFilterScopeEmpty)
    test(_TestInfoFileFilterOperatorEmpty)
    test(_TestInfoFileFilterByAmr)
