//use "math"

type _LeafNode is (String, U64)
type _ConcatNode is (Rope, Rope)
type _RopeNode is (_LeafNode | _ConcatNode)

class Rope val is Stringable, Ordered[Rope]
  var _size: U64
  var _depth: U64
  var _linebreaks: U64
  var node: _RopeNode

  new create(from: String = "", blocksize: U64 = 32) =>
    if from.size() < blocksize then
      _depth = 1
      _size = from.size()
      _linebreaks = from.count("\n")
      node = (from, blocksize)
    else
      let offset = (from.size()/2).i64()
      let left = recover Rope(from.substring(0, offset - 1), blocksize) end
      let right = recover Rope(from.substring(offset, -1), blocksize) end

      _size = left._size + right._size
      _depth = left._depth.max(right._depth) + 1
      _linebreaks = left._linebreaks + right._linebreaks
      node = (consume left, consume right)
    end

  fun val size(): U64 => _size
  fun val depth(): U64 => _depth
  fun val linecount(): U64 => _linebreaks

  /*fun box apply(index: U64): U8 ? =>
    """
    Returns the byte at the given index. Raise an error if the index is out of
    bounds.
    """*/

  fun val count(s: String): U64 =>
    """
    Counts the non-overlapping occurrences of s in the Rope.
    """
    0

  fun val find(s: String, nth: U64): I64 ? =>
    """
    Returns the index of the nth occurrence of s in this Rope. Raise an error
    if there is no n-th occurrence or if this Rope is empty.
    """
    match (node, nth)
    | ((var left: Rope, var right: Rope), var n: U64) =>
      let left_included =
        if s == "\n" then
          left._linebreaks
        else
          left.count(s)
        end

      if n <= left_included then
        left.find(s, n)
      else
        left._size.i64() + right.find(s, n - left_included)
      end
    | ((var leaf: String, _), var n: U64) =>
      leaf.find("\n" where nth = n)
    else
      error //TODO exhaustive match
    end

  fun val concat(that: Rope): Rope =>
    """
    Concatenates this Rope with that Rope.
    """
    match (this.node, that.node)
    | (("", _), _) => that
    | (_, ("", _)) => this
    else
      let rope = recover Rope end
      rope._size = this._size + that._size
      rope._depth = this._depth.max(that._depth)
      rope._linebreaks = this._linebreaks + that._linebreaks
      rope.node = (this, that)
      rope
    end

  /*fun box contains(that: Rope): Bool =>
    """
    Returns true if this Rope contains that Rope, false otherwise.
    """*/

  fun val subrope(start: I64, offset: I64): Rope =>
    """
    Returns a Rope that contains all bytes of this Rope[start, start+offset].
    Returns an empty Rope if the requested range is out of bounds.
    """
    let recurse = object
      fun tag apply(start: I64, offset: I64, node: _RopeNode): Rope =>
        match node
        | (var l: Rope, var r: Rope) =>
          let spans_left = (start <= 0) and (offset >= l.size().i64())
          let spans_right = (start <= l.size().i64()) and
            ((start + offset) >= (l.size() + r.size()).i64())

          let left =
            if spans_left then
              l
            else
              l.subrope(start, offset)
            end

          let right =
            if spans_right then
              r
            else
              r.subrope(start - l.size().i64(), offset - left.size().i64())
            end

          left.concat(right)
        | (var leaf: String, var size: U64) =>
          recover Rope(leaf.substring(start, offset), size) end
        else
          //TODO: exhaustive match
          recover Rope end
        end
    end

    if (offset <= 0) or (start >= _size.i64()) then
      recover Rope end
    else
      recurse(start, offset, node)
    end

  fun val readlines(start: U64, n: U64 = 1): Rope ? =>
    """
    Returns a Rope that holds up to n many lines starting from the line
    number denoted by start.
    """
    let marker = find("\n", start)
    let cursor = find("\n", start + n)
    let length = (marker.i64() - cursor.i64()).abs()

    subrope(marker.min(cursor).i64(), length)

  fun val position(cursor: U64): (U64, U64) /*?*/ =>
    """
    Returns the line and column position based on an absolute cursor position
    within a Rope. Raise an error if the cursor position is out of bounds.
    """
    (0, 0)

  /*fun box balance(): Rope =>
    """
    Returns an optimal Rope, i.e. its underlying tree data structure is a tree
    with the minimal external path length for a given text instance.
    """*/

  fun box string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
  =>
    """
    Converts a Rope to a String.
    """
    let len = _size
    let str = recover String(len) end

    let traverse = object
      fun tag apply(result: String iso, rope: Rope box): String iso^ =>
        match rope.node
        | (var leaf: String, _) =>
          result.append(leaf)
          consume result
        | (var left: Rope, var right: Rope) =>
          var result' = apply(consume result, left)
          apply(consume result', right)
        else
          consume result
        end
    end

    traverse(consume str, this)

  //Put this in once Fibonacci compiles (literal inference with generics)
  //fun box balanced(): Bool =>
  //  """
  //  Returns true if the given Rope is balanced, false otherwise.
  //  """
  //  Fibonacci(_depth + 2) <= _size

  fun box eq(that: Rope): Bool =>
    """
    Returns true if this Rope is structurally equivalent to that Rope,
    false otherwise.
    """
    false

  fun box lt(that: Rope): Bool =>
    """
    Returns true if all strings in this Rope are less than all strings in that
    Rope, false otherwise.
    """
    false

  /*fun box nodes(): RopeNodeIterator =>
    RopeNodeIterator(this)

  fun box leaves(): RopeLeafIterator =>
    RopeLeafIterator(this)

  fun box chars(): RopeCharIterator =>
    RopeCharIterator(this) */

  fun val lines(steps: U64 = 1): RopeLineIterator =>
    RopeLineIterator(this, steps)

/*class RopeNodeIterator is Iterator[Rope]

class RopeLeafIterator is Iterator[String]

class RopeCharIterator is Iterator[U8] */

class RopeLineIterator is Iterator[String]
  let _rope: Rope
  let _steps: U64
  var _index: U64

  new create(rope: Rope, steps: U64) =>
    _rope = rope
    _steps = steps
    _index = 0

  fun ref has_next(): Bool =>
    _index < _rope.size()

  fun ref next(): String ? =>
    let line = _rope.readlines(_index, _steps)
    _index = _index + _steps
    line.string()
