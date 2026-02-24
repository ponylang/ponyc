primitive RuleOn
  """A rule status indicating the rule is enabled."""

primitive RuleOff
  """A rule status indicating the rule is disabled."""

type RuleStatus is (RuleOn | RuleOff)
  // Whether a lint rule is enabled (RuleOn) or disabled (RuleOff).

primitive ExitSuccess
  """Exit code for a successful run with no violations."""
  fun apply(): I32 => 0

primitive ExitViolations
  """Exit code when lint violations were found."""
  fun apply(): I32 => 1

primitive ExitError
  """
  Exit code for operational errors (unreadable files, malformed config,
  lint/* suppression errors).
  """
  fun apply(): I32 => 2

type ExitCode is (ExitSuccess | ExitViolations | ExitError)
  // Process exit code: 0 for success, 1 for violations found, 2 for errors.

class val Diagnostic is (Comparable[Diagnostic] & Stringable)
  """
  A single lint diagnostic identifying a rule violation or error at a specific
  location in a source file.

  Diagnostics sort by file path, then line number, then column number, so
  output is deterministic and grouped by file.
  """
  let rule_id: String val
  let message: String val
  let file: String val
  let line: USize
  let column: USize

  new val create(
    rule_id': String val,
    message': String val,
    file': String val,
    line': USize,
    column': USize)
  =>
    rule_id = rule_id'
    message = message'
    file = file'
    line = line'
    column = column'

  fun string(): String iso^ =>
    """
    Format as `<file>:<line>:<column>: error[<rule-id>]: <message>`.
    """
    recover iso
      String.>append(file)
        .>push(':')
        .>append(line.string())
        .>push(':')
        .>append(column.string())
        .>append(": error[")
        .>append(rule_id)
        .>append("]: ")
        .>append(message)
    end

  fun eq(that: box->Diagnostic): Bool =>
    (file == that.file) and (line == that.line) and (column == that.column)
      and (rule_id == that.rule_id) and (message == that.message)

  fun lt(that: box->Diagnostic): Bool =>
    if file != that.file then
      file < that.file
    elseif line != that.line then
      line < that.line
    elseif column != that.column then
      column < that.column
    else
      rule_id < that.rule_id
    end
