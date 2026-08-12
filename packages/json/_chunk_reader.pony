use "collections"

class _ChunkReader
  """
  Byte source for the streaming parser that can hand back zero-copy views.

  Each fed chunk is held as a `val` array and never copied on feed. Bytes are
  consumed from the front; a fully-consumed chunk is dropped, so held memory is
  bounded by what has been fed but not yet consumed. Chunks live in a `List` so
  dropping the front is O(1) however many tiny chunks a value spans.

  Two cursors: the consume cursor (bytes before it are gone) and a single forward
  scan cursor used to find a string's extent before deciding whether to view or
  decode it. The scan cursor is a node reference, so it advances in O(1) and
  survives chunks being dropped behind it. Only one leaf scans at a time, so one
  scan cursor suffices; it is valid from `scan_reset()` until the next consume.
  When a string lies within a single chunk, `view()` returns a `trim`-backed
  `String` with no copy.
  """
  embed _chunks: List[Array[U8] val]
  var _head: USize = 0        // consumed offset within the front chunk
  var _available: USize = 0

  var _scan_node: (ListNode[Array[U8] val] | None) = None
  var _scan_off: USize = 0
  var _scan_len: USize = 0

  new create() =>
    _chunks = List[Array[U8] val]

  fun ref append(data: ByteSeq) =>
    """Hold a chunk as `val` — no copy. Empty chunks are dropped."""
    let arr =
      match data
      | let s: String => s.array()
      | let a: Array[U8] val => a
      end
    if arr.size() > 0 then
      _chunks.push(arr)
      _available = _available + arr.size()
    end

  fun size(): USize =>
    _available

  fun peek(): U8 ? =>
    """The byte at the consume cursor."""
    if _available == 0 then error end
    _chunks.head()?()?(_head)?

  fun ref skip1() =>
    """Consume one byte from the front."""
    if _available == 0 then _Unreachable(); return end
    _head = _head + 1
    _available = _available - 1
    try
      if _head >= _chunks.head()?()?.size() then
        _chunks.shift()?
        _head = 0
      end
    else _Unreachable()
    end

  fun ref skip(n: USize) =>
    """Consume `n` bytes from the front, dropping exhausted chunks."""
    var rem = n
    try
      while rem > 0 do
        let avail = _chunks.head()?()?.size() - _head
        if avail > rem then
          _head = _head + rem
          _available = _available - rem
          rem = 0
        else
          _chunks.shift()?
          _head = 0
          _available = _available - avail
          rem = rem - avail
        end
      end
    else _Unreachable()
    end

  // --- forward scan (no consume) ------------------------------------------

  fun ref scan_reset() =>
    """Place the scan cursor at the consume cursor for a fresh scan."""
    _scan_node = try _chunks.head()? else None end
    _scan_off = _head
    _scan_len = 0

  fun scan_at_end(): Bool =>
    """True once the scan cursor has reached the end of the fed bytes."""
    _scan_len >= _available

  fun ref scan_byte(): U8 ? =>
    """
    The byte at the scan cursor. Errors if the scan is at the end. Picks up chunks
    fed after a suspend: acquires the head if the scan was reset on an empty list,
    and hops onto the next chunk at a boundary.
    """
    if scan_at_end() then error end
    var node =
      match _scan_node
      | let n: ListNode[Array[U8] val] => n
      | None => let h = _chunks.head()?; _scan_node = h; h
      end
    if _scan_off >= node()?.size() then
      // scan_at_end() being false guarantees a next chunk exists here.
      node = try node.next() as ListNode[Array[U8] val]
        else _Unreachable(); node end
      _scan_node = node
      _scan_off = 0
    end
    node()?(_scan_off)?

  fun ref scan_advance() =>
    """Advance the scan cursor one byte (the chunk hop happens in scan_byte)."""
    _scan_len = _scan_len + 1
    _scan_off = _scan_off + 1

  fun scan_len(): USize =>
    """Bytes from the consume cursor to the scan cursor."""
    _scan_len

  fun scan_single_chunk(): Bool =>
    """True if the span [consume cursor, scan cursor) lies within one chunk."""
    try
      match _scan_node
      | let s: ListNode[Array[U8] val] box => _chunks.head()? is s
      | None => false
      end
    else false
    end

  fun view(): (String val | None) =>
    """
    A zero-copy view of [consume cursor, scan cursor) if it lies within one chunk,
    else `None`. The returned `String` shares the fed chunk's memory (the chunk's
    backing block stays live as long as the `String` is held).
    """
    try
      match _scan_node
      | let s: ListNode[Array[U8] val] box =>
        let head = _chunks.head()?
        if head is s then
          String.from_array(head()?.trim(_head, _scan_off))
        else
          None
        end
      | None => None
      end
    else None
    end
