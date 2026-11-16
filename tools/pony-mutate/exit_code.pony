primitive _ExitSuccess
  """
  Exit code for a successful run: all mutants were killed (or none to run).
  """
  fun apply(): I32 => 0

primitive _ExitSurvivors
  """
  Exit code when one or more mutants survived.
  """
  fun apply(): I32 => 1

primitive _ExitError
  """
  Exit code for operational errors (bad arguments, baseline failure,
  I/O error).
  """
  fun apply(): I32 => 2
