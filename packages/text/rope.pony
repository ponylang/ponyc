type _LeafNode is (String, U64)
type _ConcatNode is (Rope, Rope)
type _RopeNode is (_LeafNode | _ConcatNode)

interface _Collector
  fun ref apply(string: String box): String ref

class Rope val is Stringable, Ordered[Rope]
  var _size: U64
  var _depth: U64
  var _linebreaks: U64
  var node: _RopeNode

  new create(from: String = "", blocksize: U64 = 32) =>
    if from.size() < blocksize then
      _depth = 1
      _size = from.size()
      _linebreaks = from.count("\n");
      node = (from, blocksize)
    else
      let offset = (from.size()/2).i64()
      let left = recover Rope(from.substring(0, offset - 1), blocksize) end
      let right = recover Rope(from.substring(offset, -1), blocksize) end

      _size = left._size + right._size
      _depth = left._depth.max(right._depth)
      _linebreaks = left._linebreaks + right._linebreaks
      node = (consume left, consume right)
    end

  fun box size(): U64 => _size
  fun box depth(): U64 => _depth
  fun box linebreaks(): U64 => _linebreaks

  /*fun box apply(index: U64): U8 ? =>
    """
    Returns the byte at the given index. Raise an error if the index is out of
    bounds.
    """*/

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

  fun box subrope(start: I64, length: I64): Rope =>
    """
    Returns a Rope that contains all bytes of this Rope within
    [start, start+length].
    """
    recover Rope end

  fun box position(cursor: U64): (U64, U64) /*?*/ =>
    """
    Returns the line and column position based on an absolute cursor position
    within a Rope. Raise an error if the cursor position is out of bounds.
    """
    (U64(0), U64(0))

  /*fun box balance(): Rope =>
    """
    Returns an optimal Rope, i.e. its underlying tree data structure is a tree
    with the minimal external path length for a given text instance.
    """*/

  fun box string(): String =>
    """
    Converts a Rope to a String.
    """
    let len = _size
    let str = recover String.prealloc(len) end

    let traverse = object
      fun tag apply(result: String iso!, rope: Rope box) =>
        match rope.node
        | (var leaf: String, _) =>
          result.append(leaf)
        | (var left: Rope, var right: Rope) =>
          apply(result, left)
          apply(result, right)
        end
    end

    traverse(str, this)
    consume str

  fun box eq(that: Rope): Bool =>
    """
    Returns true if this is structurally equivalent to that, false otherwise.
    """
    false

  fun box lt(that: Rope): Bool =>
    """
    Returns true if all strings in this are less than all strings in that, false
    otherwise.
    """
    false

  /*fun box nodes(): RopeNodeIterator =>
    RopeNodeIterator(this)

  fun box leaves(): RopeLeafIterator =>
    RopeLeafIterator(this)

  fun box chars(): RopeCharIterator =>
    RopeCharIterator(this)

  fun box lines(): RopeLineIterator =>
    RopeLineIterator(this)

class RopeNodeIterator is Iterable[Rope]

class RopeLeafIterator is Iterable[String]

class RopeCharIterator is Iterable[U8]

class RopeLineIterator is Iterable[String]*/
