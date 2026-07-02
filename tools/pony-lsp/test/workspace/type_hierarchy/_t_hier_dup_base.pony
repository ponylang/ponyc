trait _THierDupBase
  fun f_dup(): None

class _THierDupClass is (_THierDupBase & _THierDupBase)
  fun f_dup(): None => None
