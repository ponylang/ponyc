//use "math"

type _Node is ((Rope box, Rope box) | String)

class Rope val is Stringable
  let _node: _Node
  let _depth: U64
  let _size: U64
  let _linebreaks: U64
  let _blocksize: U64

  new create(from: String = "", blocksize': U64 = 32) =>
    """
    Creates a new Rope from a String.
    """
    _blocksize = blocksize'
    let len = from.size()

    if len < blocksize' then
      _depth = 1
      _size = len
      _linebreaks = from.count("\n")
      _node = from
    else
      let splitsize = len.i64()/2
      let left = recover Rope(from.substring(0, splitsize - 1), blocksize') end
      let right = recover Rope(from.substring(splitsize, -1), blocksize') end

      _depth = left._depth.max(right._depth + 1)
      _size = left._size + right._size
      _linebreaks = left._linebreaks + right._linebreaks
      _node = (consume left, consume right)
    end

  new _internal(depth': U64, size': U64, linebreaks': U64, blocksize': U64,
    node: _Node)
    =>
    _depth = depth'
    _size = size'
    _linebreaks = linebreaks'
    _blocksize = blocksize'
    _node = node

  fun box _linebreak(nth: U64): U64 ? =>
    if nth == 0 then return 0 end

    match (_node, nth)
    | ((var left: Rope box, var right: Rope box), var n: U64) =>
      if n <= left._linebreaks then
        left._linebreak(n)
      else
        left._size + right._linebreak(n - left._linebreaks)
      end
    | (var leaf: String, var n: U64) =>
        leaf.find("\n" where nth = n).u64()
    else
      error //remove, exhaustive match
    end

  fun box depth(): U64 => _depth
  fun box size(): U64 => _size
  fun box linebreaks(): U64 => _linebreaks
  fun box blocksize(): U64 => _blocksize

  fun box concat(with: this->Rope ref): this->Rope ref =>
    """
    Concatenates two Ropes.
    """
    match (left._size, with._size)
    | (_, 0) => this
    | (0, _) => right
    else
      let depth' = _depth.max(rope._depth) + 1
      let size' = _size + rope._size
      let linebreaks' = _linebreaks + rope._linebreaks
      let blocksize' = _blocksize.max(rope._blocksize)

      Rope(depth', size', linebreaks', blocksize', (this, with))
    end

  fun box subrope(start: I64, length: I64): Rope =>
    """
    Returns a Rope that holds length many characters starting from start.
    """

  fun box readlines(start: U64, count: U64 = 1): Rope =>
    """
    Returns a Rope that holds up to count many lines starting from the line
    number denoted by start.
    """
    let marker = _linebreak(start)
    let cursor = _linebreak(start + count)
    let length = (marker.i64() = cursor.i64()).abs()

    subrope(marker.min(cursor).i64(), length)

  fun box position(cursor: U64): (U64, U64) ? =>
    """
    Returns the line and column position based on an absolute cursor position
    within a Rope. Raise an error if the cursor position is out of bounds.
    """
    let y = subrope(0, cursor.i64())
    let x = try subrope(0, _linebreak(y._linebreaks - 1)) else this end

    let line = y._linebreaks
    let column = y._size - x._size

    (line, column)

  //fun box balance(): Rope =>
  //  """
  //  Returns an optimal Rope, i.e. its underlying tree data structure is a tree
  //  with the minimal external path length for a given text instance.
  //  """

  //Put this in once Fibonacci compiles (literal inference with generics)
  //fun box balanced(): Bool =>
  //  """
  //  Returns true if the given Rope is balanced, false otherwise.
  //  """
  //  Fibonacci(depth + 2) <= _size

  fun box lines(): RopeLines =>
    """
    Returns an Iterator to enumerate the contents of a rope line by line.
    """
    RopeLines(this)

  fun box string(): String =>
    """
    Converts a Rope to a String.
    """
    //TODO
    //we can make this faster once we support object literals
    //and inline functions, because then stringifying a Rope
    //requires exactly one allocation (String.prealloc(_size)),
    //and then call the inline function resursively, instead of
    //string().
    var text = recover String end

    match _node
    | var leaf: String =>
      text.append(leaf)
    | (var right: Rope, var left: Rope) =>
      text.append(left.string())
      text.append(right.string())
    end

    consume text

class RopeLines is Iterator[String]
  let _rope: Rope box
  var _index: U64

  new create(rope': Rope box) =>
    _rope = rope'
    _index = 0

  fun ref has_next(): Bool =>
    index < _rope.size()

  fun ref next(): String =>
    let line = _rope.readlines(index)
    index = index + 1
    line.string()
