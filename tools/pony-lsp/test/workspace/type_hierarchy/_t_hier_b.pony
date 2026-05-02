trait _THierA
  """
  Root of the linear hierarchy chain. No supertypes; subtypes = [_THierB].
  """
  fun f_a(): None

trait _THierB is _THierA
  """
  Middle of the linear hierarchy chain. Supertypes = [_THierA];
  subtypes = [_THierC].
  """
  fun f_b(): None

class _THierC is _THierB
  """
  Leaf of the linear hierarchy chain. Supertypes = [_THierB]; no subtypes.
  """
  fun f_a(): None => None
  fun f_b(): None => None
