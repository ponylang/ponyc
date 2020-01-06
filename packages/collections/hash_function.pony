interface val HashFunction[A]
  """
  A pluggable hash function.
  """
  new val create()
    """
    Data structures create instances internally. Use a primitive if possible.
    """

  fun hash(x: box->A!): USize
    """
    Calculate the hash of some type. This is an alias of the type parameter to
    allow data structures to hash things without consuming them.
    """

  fun eq(x: box->A!, y: box->A!): Bool
    """
    Determine equality between two keys with the same hash. This is done with
    viewpoint adapted aliases to allow data structures to determine equality
    in a box fun without consuming keys.
    """

interface val HashFunction64[A]
  """
  A pluggable hash function with 64-bit hashes.
  """
  new val create()
    """
    Data structures create instances internally. Use a primitive if possible.
    """

  fun hash64(x: box->A!): U64
    """
    Calculate the hash of some type. This is an alias of the type parameter to
    allow data structures to hash things without consuming them.
    """

  fun eq(x: box->A!, y: box->A!): Bool
    """
    Determine equality between two keys with the same hash. This is done with
    viewpoint adapted aliases to allow data structures to determine equality
    in a box fun without consuming keys.
    """

primitive HashEq[A: (Hashable #read & Equatable[A] #read)] is HashFunction[A]
  fun hash(x: box->A): USize =>
    """
    Use the hash function from the type parameter.
    """
    x.hash()

  fun eq(x: box->A, y: box->A): Bool =>
    """
    Use the structural equality function from the type parameter.
    """
    x == y

primitive HashEq64[A: (Hashable64 #read & Equatable[A] #read)] is
  HashFunction64[A]
  fun hash64(x: box->A): U64 =>
    """
    Use the hash function from the type parameter.
    """
    x.hash64()

  fun eq(x: box->A, y: box->A): Bool =>
    """
    Use the structural equality function from the type parameter.
    """
    x == y

primitive HashIs[A] is (HashFunction[A] & HashFunction64[A])
  fun hash(x: box->A!): USize =>
    """
    Hash the identity rather than the contents.
    """
    (digestof x).hash()

  fun hash64(x: box->A!): U64 =>
    """
    Hash the identity rather than the contents.
    """
    (digestof x).hash64()

  fun eq(x: box->A!, y: box->A!): Bool =>
    """
    Determine equality by identity rather than structurally.
    """
    x is y

primitive HashByteSeq is
  (HashFunction[ByteSeq box] & HashFunction64[ByteSeq box])
  """
  Hash and equality functions for arbitrary ByteSeq.
  """
  fun hash(x: ByteSeq box): USize =>
    @ponyint_hash_block[USize](x.cpointer(), x.size())

  fun hash64(x: ByteSeq box): U64 =>
    @ponyint_hash_block64[U64](x.cpointer(), x.size())

  fun eq(x: ByteSeq box, y: ByteSeq box): Bool =>
    if x.size() == y.size() then
      @memcmp[I32](x.cpointer(), y.cpointer(), x.size()) == 0
    else
      false
    end
