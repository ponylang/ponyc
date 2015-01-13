class Identity[A] is Hashable, Comparable[Identity[A]]
  """
  This creates a hashable, comparable type from an identity.
  """
  let data: A

  new create(data': A) =>
    """
    Create a hashable identity for the given data.
    """
    data = consume data'

  fun hash(): U64 =>
    """
    Generate a hash based on the identity of the data.
    """
    (&data).hash()

  fun eq(that: Identity[A] box): Bool =>
    """
    Check the identity of the data.
    """
    data is that.data

  fun ne(that: Identity[A] box): Bool =>
    """
    Check the identity of the data.
    """
    data isnt that.data
