class val _Literal
  """
  A literal text segment of a URI template.

  Literal text is copied verbatim to the expansion output.
  """
  let text: String val

  new val create(text': String val) =>
    text = text'

class val _Expression
  """
  An expression segment of a URI template.

  Contains an operator and one or more variable specifications that are
  expanded against a variable set during template expansion.
  """
  let operator: _Operator
  let varspecs: Array[_VarSpec] val

  new val create(operator': _Operator, varspecs': Array[_VarSpec] val) =>
    operator = operator'
    varspecs = varspecs'

// A segment of a parsed URI template: either literal text or an expression.
type _TemplatePart is (_Literal | _Expression)
