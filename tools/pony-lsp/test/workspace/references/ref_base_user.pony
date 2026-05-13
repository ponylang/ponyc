use "ref_base"

class RefBaseUser
  """
  Uses RefBase from the ref_base sub-package for cross-package reference
  tests.
  """
  fun use_it(obj: RefBase): U32 =>
    obj.value()
