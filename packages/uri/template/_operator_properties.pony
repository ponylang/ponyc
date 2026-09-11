primitive _OperatorProperties
  """
  Encodes the RFC 6570 operator properties table.

  Each operator has a set of properties that control how expressions using
  that operator are expanded: the prefix before the first value, the separator
  between values, whether name=value form is used, the suffix for empty
  named values, and whether reserved characters are passed through.
  """
  fun first(op: _Operator): String =>
    match \exhaustive\ op
    | _OpNone      => ""
    | _OpPlus      => ""
    | _OpHash      => "#"
    | _OpDot       => "."
    | _OpSlash     => "/"
    | _OpSemicolon => ";"
    | _OpQuestion  => "?"
    | _OpAmpersand => "&"
    end

  fun sep(op: _Operator): String =>
    match \exhaustive\ op
    | _OpNone      => ","
    | _OpPlus      => ","
    | _OpHash      => ","
    | _OpDot       => "."
    | _OpSlash     => "/"
    | _OpSemicolon => ";"
    | _OpQuestion  => "&"
    | _OpAmpersand => "&"
    end

  fun named(op: _Operator): Bool =>
    match \exhaustive\ op
    | _OpNone      => false
    | _OpPlus      => false
    | _OpHash      => false
    | _OpDot       => false
    | _OpSlash     => false
    | _OpSemicolon => true
    | _OpQuestion  => true
    | _OpAmpersand => true
    end

  fun ifemp(op: _Operator): String =>
    match \exhaustive\ op
    | _OpNone      => ""
    | _OpPlus      => ""
    | _OpHash      => ""
    | _OpDot       => ""
    | _OpSlash     => ""
    | _OpSemicolon => ""
    | _OpQuestion  => "="
    | _OpAmpersand => "="
    end

  fun allow_reserved(op: _Operator): Bool =>
    match \exhaustive\ op
    | _OpNone      => false
    | _OpPlus      => true
    | _OpHash      => true
    | _OpDot       => false
    | _OpSlash     => false
    | _OpSemicolon => false
    | _OpQuestion  => false
    | _OpAmpersand => false
    end
