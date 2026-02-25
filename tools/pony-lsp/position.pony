use "immutable-json"
use "pony_compiler"

class val LspPositionRange
    """
    See: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#range
    """
    let _start: LspPosition
        """
        Inclusive start position
        """
    let _end: LspPosition
        """
        Exclusive end position
        """

    new val from_single_pos(pos: LspPosition) =>
        _start = pos
        _end = pos

    new val create(start_pos: LspPosition, end_pos: LspPosition) =>
        _start = start_pos
        _end = end_pos

    fun to_json(): JsonType =>
        Obj("start", this._start.to_json())(
            "end", this._end.to_json()
        ).build()

class val LspPosition
    """
    See: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position
    """
    let _line: USize
      """
      Line position in a document (zero-based).
      """
    let _character: USize

    new val create(line': USize, character': USize) =>
        _line = line'
        _character = character'

    new val from_ast_pos(position: Position) =>
        _line = (position.line() - 1).max(0)
        _character = (position.column() - 1).max(0)

    new val from_ast_pos_end(position: Position) =>
        """
        Convert AST position to LSP position for range end (exclusive).
        AST positions are inclusive (point to the last character),
        but LSP range ends are exclusive (point one past the last character).
        """
        _line = (position.line() - 1).max(0)
        _character = position.column().max(0)  // Don't subtract 1 for end position

    fun to_json(): JsonType =>
        Obj("line", this._line.i64())(
            "character", this._character.i64()).build()

class val LspLocation
    """
    See: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#location
    """
    let uri: String
    let range: LspPositionRange

    new val create(uri': String, range': LspPositionRange) =>
        uri = uri'
        range = range'

    fun to_json(): JsonType =>
        Obj("uri", uri)("range", this.range.to_json()).build()
