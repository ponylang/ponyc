use "collections"

class val Position is (Comparable[Position] & Hashable & Stringable)
  """
  Position in a module.

  Line number and line column are 1-based.
  """
  let _line: USize
  let _column: USize

  new val create(line': USize, column': USize) =>
    _line = line'
    _column = column'

  new val min() =>
    """
    Minimum Position.

    Cannot be a valid position in a document.
    """
    _line = USize.min_value()
    _column = USize.min_value()

  new val max() =>
    """
    Maximum Position.

    Will very likely not be a valid position in a document.
    """
    _line = USize.max_value()
    _column = USize.max_value()

  fun eq(other: box->Position): Bool =>
    (_line == other._line) and (_column == other._column)

  fun lt(other: box->Position): Bool =>
    (_line < other._line) or ((_line == other._line) and (_column < other._column))

  fun hash(): USize =>
    _line.hash() xor _column.hash() // TODO: better hashing support for Pony

  fun string(): String iso^ =>
    recover iso
      String .> append(_line.string() + ":" + _column.string())
    end

  fun line(): USize => _line
  fun column(): USize => _column

// TODO: needed?
class val FilePosition is (Comparable[FilePosition] & Hashable & Stringable)
  """
  Position in a certain file.
  """
  let position: Position val
  let file: String val

  new val create(position': Position, file': String val) =>
    position = position'
    file = file'

  fun eq(other: box->FilePosition): Bool =>
    (position == other.position) and (file == other.file)

  fun lt(other: box->FilePosition): Bool =>
    (file == other.file) and (position < other.position)

  fun hash(): USize =>
    position.hash() xor file.hash()

  fun string(): String iso^ =>
    recover iso
      String .> append(file + " " + position.string())
    end

  fun line(): USize => position.line()
  fun column(): USize => position.column()
