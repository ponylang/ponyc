class val JsonStreamLimits
  """
  Resource bounds for `JsonStreamParser`.

  Streaming parsers are typically fed from untrusted sources (a socket, a file
  of unknown origin), and each value is materialized into a `JsonValue` tree
  whose size the producer controls. These limits cap what a single value can
  consume so a malicious or malformed input cannot exhaust memory. Each limit is
  checked *during* parsing, so an oversized or unterminated value is rejected
  near the moment it crosses the bound rather than only at the end. Enforcement
  is approximate: a value may exceed a limit by a small, bounded amount — a
  trailing `}`/`]`, or a final multi-byte `\uXXXX` expansion — before the next
  check rejects it.

  The default constructor applies conservative limits; raise a specific bound
  when a trusted source legitimately has a large value or deep nesting.

  ```pony
  // Default conservative limits:
  let parser = JsonStreamParser

  // Custom limits:
  let parser = JsonStreamParser(JsonStreamLimits(where max_depth' = 32))
  ```
  """
  let max_depth: USize
    """
    Maximum container nesting depth. This also protects *downstream* consumers:
    `JsonPrinter` and JSONPath recursive descent (`..`) walk the tree with
    native recursion, so a value nested tens of thousands of levels deep can
    overflow the native stack when they process it. The default bound stays well
    under that, keeping streamed values safe to hand to those APIs; raising it
    erodes that margin.
    """

  let max_string_len: USize
    """Maximum decoded length, in bytes, of a single string."""

  let max_number_len: USize
    """Maximum length, in bytes, of a single number's text."""

  let max_value_bytes: USize
    """Maximum total source bytes consumed for a single top-level value."""

  new val create(
    max_depth': USize = 1024,
    max_string_len': USize = 1_048_576,
    max_number_len': USize = 256,
    max_value_bytes': USize = 8_388_608)
  =>
    max_depth = max_depth'
    max_string_len = max_string_len'
    max_number_len = max_number_len'
    max_value_bytes = max_value_bytes'
