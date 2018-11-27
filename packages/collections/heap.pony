
type MinHeap[A: Comparable[A] #read] is BinaryHeap[A, MinHeapPriority[A]]
type MaxHeap[A: Comparable[A] #read] is BinaryHeap[A, MaxHeapPriority[A]]

class BinaryHeap[A: Comparable[A] #read, P: BinaryHeapPriority[A]]
  """
  A priority queue implemented as a binary heap. The `BinaryHeapPriority` type
  parameter determines whether this is max-heap or a min-heap.
  """
  embed _data: Array[A]

  new create(len: USize) =>
    """
    Create an empty heap with space for `len` elements.
    """
    _data = Array[A](len)

  fun ref clear() =>
    """
    Remove all elements from the heap.
    """
    _data.clear()

  fun size(): USize =>
    """
    Return the number of elements in the heap.
    """
    _data.size()

  fun peek(): this->A ? =>
    """
    Return the highest priority item in the heap. For max-heaps, the greatest
    item will be returned. For min-heaps, the smallest item will be returned.
    """
    _data(0)?

  fun ref push(value: A) =>
    """
    Push an item into the heap.

    The time complexity of this operation is O(log(n)) with respect to the size
    of the heap.
    """
    _data.push(value)
    _sift_up(size() - 1)

  fun ref pop(): A^ ? =>
    """
    Remove the highest priority value from the heap and return it. For
    max-heaps, the greatest item will be returned. For min-heaps, the smallest
    item will be returned.

    The time complexity of this operation is O(log(n)) with respect to the size
    of the heap.
    """
    let n = size() - 1
    _data.swap_elements(0, n)?
    _sift_down(0, n)
    _data.pop()?

  fun ref append(
    seq: (ReadSeq[A] & ReadElement[A^]),
    offset: USize = 0,
    len: USize = -1)
  =>
    """
    Append len elements from a sequence, starting from the given offset.
    """
    _data.append(seq, offset, len)
    _make_heap()

  fun ref concat(iter: Iterator[A^], offset: USize = 0, len: USize = -1) =>
    """
    Add len iterated elements, starting from the given offset.
    """
    _data.concat(iter, offset, len)
    _make_heap()

  fun values(): ArrayValues[A, this->Array[A]]^ =>
    """
    Return an iterator for the elements in the heap. The order of elements is
    arbitrary.
    """
    _data.values()

  fun ref _make_heap() =>
    let n = size()
    if n < 2 then return end
    var i = (n / 2)
    while (i = i - 1) > 0 do
      _sift_down(i, n)
    end

  fun ref _sift_up(n: USize) =>
    var idx = n
    try
      while true do
        let parent_idx = (idx - 1) / 2
        if (parent_idx == idx) or not P(_data(idx)?, _data(parent_idx)?) then
          break
        end
        _data.swap_elements(parent_idx, idx)?
        idx = parent_idx
      end
    end

  fun ref _sift_down(start: USize, n: USize): Bool =>
    var idx = start
    try
      while true do
        var left = (2 * idx) + 1
        if (left >= n) or (left < 0) then
          break
        end
        let right = left + 1
        if (right < n) and P(_data(right)?, _data(left)?) then
          left = right
        end
        if not P(_data(left)?, _data(idx)?) then
          break
        end
        _data.swap_elements(idx, left)?
        idx = left
      end
    end
    idx > start

  fun _apply(i: USize): this->A ? =>
    _data(i)?

type BinaryHeapPriority[A: Comparable[A] #read] is
  ( _BinaryHeapPriority[A]
  & (MinHeapPriority[A] | MaxHeapPriority[A]))

interface val _BinaryHeapPriority[A: Comparable[A] #read]
  new val create()
  fun apply(x: A, y: A): Bool

primitive MinHeapPriority[A: Comparable[A] #read] is _BinaryHeapPriority[A]
  fun apply(x: A, y: A): Bool =>
    x < y

primitive MaxHeapPriority [A: Comparable[A] #read] is _BinaryHeapPriority[A]
  fun apply(x: A, y: A): Bool =>
    x > y
