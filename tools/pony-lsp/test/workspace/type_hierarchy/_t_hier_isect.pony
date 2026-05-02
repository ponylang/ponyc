trait _THierLeft
  """
  Left branch of the intersection in _THierIsect.
  """
  fun f_l(): None

trait _THierRight
  """
  Right branch of the intersection in _THierIsect.
  """
  fun f_r(): None

trait _THierIsect is (_THierLeft & _THierRight)
  """
  Fixture for intersection-type provides traversal. Supertypes should return
  both _THierLeft and _THierRight, demonstrating that the server recurses
  through tk_isecttype nodes in the provides clause.
  """
  fun f_l(): None
  fun f_r(): None
