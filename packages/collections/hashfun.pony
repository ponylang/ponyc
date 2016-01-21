interface Hashable
  """
  Anything with a hash method is hashable.
  """
  fun hash(): U64

interface val HashFunction[A]
  """
  A pluggable hash function.
  """
  new val create()
    """
    Data structures create instances internally. Use a primitive if possible.
    """

  fun hash(x: box->A!): U64
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
  fun hash(x: box->A): U64 =>
    """
    Use the hash function from the type parameter.
    """
    x.hash()

  fun eq(x: box->A, y: box->A): Bool =>
    """
    Use the structural equality function from the type parameter.
    """
    x == y

primitive HashIs[A] is HashFunction[A]
  fun hash(x: box->A!): U64 =>
    """
    Hash the identity rather than the contents.
    """
    (identityof x).hash()

  fun eq(x: box->A!, y: box->A!): Bool =>
    """
    Determine equality by identity rather than structurally.
    """
    x is y

primitive HashByteSeq
  """
  Hash and equality functions for arbitrary ByteSeq.
  """
  fun hash(x: ByteSeq): U64 =>
    @hash_block[U64](x.cstring(), x.size())

  fun eq(x: ByteSeq, y: ByteSeq): Bool =>
    if x.size() == y.size() then
      @memcmp[I32](x.cstring(), y.cstring(), x.size()) == 0
    else
      false
    end
