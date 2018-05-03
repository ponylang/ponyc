interface val SourceLoc
  """
  Represents a location in a Pony source file, as reported by `__loc`.
  """
  fun file(): String
    """
    Name and path of source file.
    """

  fun type_name(): String
    """
    Name of nearest class, actor, primitive, struct, interface, or trait.
    """

  fun method_name(): String
    """
    Name of containing method.
    """

  fun line(): USize
    """
    Line number within file.
    Line numbers start at 1.
    """

  fun pos(): USize
    """
    Character position on line.
    Character positions start at 1.
    """
