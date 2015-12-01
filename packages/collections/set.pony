type Set[A: (Hashable #read & Equatable[A] #read)] is HashSet[A, HashEq[A]]

type SetIs[A] is HashSet[A, HashIs[A!]]

class HashSet[A, H: HashFunction[A!] val] is Comparable[HashSet[A, H] box]
  """
  A set, built on top of a HashMap. This is implemented as map of an alias of
  a type to itself
  """
  let _map: HashMap[A!, A, H]

  new create(prealloc: USize = 8) =>
    """
    Defaults to a prealloc of 8.
    """
    _map = _map.create(prealloc)

  fun size(): USize =>
    """
    The number of items in the set.
    """
    _map.size()

  fun space(): USize =>
    """
    The available space in the set.
    """
    _map.space()

  fun apply(value: box->A!): this->A ? =>
    """
    Return the value if its in the set, otherwise raise an error.
    """
    _map(value)

  fun ref clear(): HashSet[A, H]^ =>
    """
    Remove all elements from the set.
    """
    _map.clear()
    this

  fun ref set(value: A): HashSet[A, H]^ =>
    """
    Add a value to the set.
    """
    _map(value) = consume value
    this

  fun ref unset(value: box->A!): HashSet[A, H]^ =>
    """
    Remove a value from the set.
    """
    try _map.remove(value) end
    this

  fun ref extract(value: box->A!): A^ ? =>
    """
    Remove a value from the set and return it. Raises an error if the value
    wasn't in the set.
    """
    _map.remove(value)._2

  fun ref concat(iter: Iterator[A^]): HashSet[A, H]^ =>
    """
    Add everything in iter to the set.
    """
    for value in iter do
      set(consume value)
    end

    this

  fun ref union[K: HashFunction[A] val = H](that: HashSet[A^, K]):
    HashSet[A, H]^
  =>
    """
    Add everything in that to the set.
    """
    for value in that.values() do
      set(consume value)
    end

    this

  fun ref intersect[K: HashFunction[box->A!] val = H](
    that: HashSet[box->A!, K]): HashSet[A, H]^
  =>
    """
    Remove everything that isn't in that.
    """
    for value in values() do
      try
        that(value)
      else
        unset(value)
      end
    end

    this

  fun ref difference[K: HashFunction[A] val = H](
    that: HashSet[A^, K]): HashSet[A, H]^
  =>
    """
    Remove elements in this which are also in that. Add elements in that which
    are not in this.
    """
    for value in that.values() do
      try
        extract(value)
      else
        set(consume value)
      end
    end

    this

  fun ref remove[K: HashFunction[box->A!] val = H](
    that: HashSet[box->A!, K]): HashSet[A, H]^
  =>
    """
    Remove everything that is in that.
    """
    for value in that.values() do
      unset(value)
    end

    this

  fun add[K: HashFunction[this->A!] val = H](value: this->A!):
    HashSet[this->A!, K]^
  =>
    """
    Add a value to the set.
    """
    clone[K]().set(value)

  fun sub[K: HashFunction[this->A!] val = H](value: this->A!):
    HashSet[this->A!, K]^
  =>
    """
    Remove a value from the set.
    """
    clone[K]().unset(value)

  fun op_or[K: HashFunction[this->A!] val = H](that: HashSet[A, H] box):
    HashSet[this->A!, K]^
  =>
    """
    Create a set with the elements of both this and that.
    """
    let r = clone[K]()

    for value in that.values() do
      r.set(value)
    end
    r

  fun op_and[K: HashFunction[this->A!] val = H](that: HashSet[A, H] box):
    HashSet[this->A!, K]^
  =>
    """
    Create a set with the elements that are in both this and that.
    """
    let r = HashSet[this->A!, K](size().min(that.size()))

    for value in values() do
      try
        that(value)
        r.set(value)
      end
    end
    r

  fun op_xor[K: HashFunction[this->A!] val = H](that: HashSet[A, H] box):
    HashSet[this->A!, K]^
  =>
    """
    Create a set with the elements that are in either set but not both.
    """
    let r = HashSet[this->A!, K](size().max(that.size()))

    for value in values() do
      try
        that(value)
      else
        r.set(value)
      end
    end

    for value in that.values() do
      try
        this(value)
      else
        r.set(value)
      end
    end
    r

  fun without[K: HashFunction[this->A!] val = H](that: HashSet[A, H] box):
    HashSet[this->A!, K]^
  =>
    """
    Create a set with the elements of this that are not in that.
    """
    let r = HashSet[this->A!, K](size())

    for value in values() do
      try
        that(value)
      else
        r.set(value)
      end
    end
    r

  fun clone[K: HashFunction[this->A!] val = H](): HashSet[this->A!, K]^ =>
    """
    Create a clone. The element type may be different due to aliasing and
    viewpoint adaptation.
    """
    let r = HashSet[this->A!, K](size())

    for value in values() do
      r.set(value)
    end
    r

  fun eq(that: HashSet[A, H] box): Bool =>
    """
    Returns true if the sets contain the same elements.
    """
    (size() == that.size()) and (this <= that)

  fun ne(that: HashSet[A, H] box): Bool =>
    """
    Returns false if the sets contain the same elements.
    """
    not (this == that)

  fun lt(that: HashSet[A, H] box): Bool =>
    """
    Returns true if every element in this is also in that, and this has fewer
    elements than that.
    """
    (size() < that.size()) and (this <= that)

  fun le(that: HashSet[A, H] box): Bool =>
    """
    Returns true if every element in this is also in that.
    """
    try
      for value in values() do
        that(value)
      end
      true
    else
      false
    end

  fun gt(that: HashSet[A, H] box): Bool =>
    """
    Returns true if every element in that is also in this, and this has more
    elements than that.
    """
    (size() > that.size()) and (that <= this)

  fun ge(that: HashSet[A, H] box): Bool =>
    """
    Returns true if every element in that is also in this.
    """
    that <= this

  fun next_index(prev: USize = -1): USize ? =>
    """
    Given an index, return the next index that has a populated value. Raise an
    error if there is no next populated index.
    """
    _map.next_index(prev)

  fun index(i: USize): this->A ? =>
    """
    Returns the value at a given index. Raise an error if the index is not
    populated.
    """
    _map.index(i)._2

  fun values(): SetValues[A, H, this->HashSet[A, H]]^ =>
    """
    Return an iterator over the values.
    """
    SetValues[A, H, this->HashSet[A, H]](this)

class SetValues[A, H: HashFunction[A!] val, S: HashSet[A, H] #read] is
  Iterator[S->A]
  """
  An iterator over the values in a set.
  """
  let _set: S
  var _i: USize = -1
  var _count: USize = 0

  new create(set: S) =>
    """
    Creates an iterator for the given set.
    """
    _set = set

  fun has_next(): Bool =>
    """
    True if it believes there are remaining entries. May not be right if values
    were added or removed from the set.
    """
    _count < _set.size()

  fun ref next(): S->A ? =>
    """
    Returns the next value, or raises an error if there isn't one. If values
    are added during iteration, this may not return all values.
    """
    _i = _set.next_index(_i)
    _count = _count + 1
    _set.index(_i)
