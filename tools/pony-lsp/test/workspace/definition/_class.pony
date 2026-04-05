class _Class
  """
  A class for testing goto definition.
  """
  let _value: U32

  new create(v: U32) =>
    _value = v

  fun get(): U32 =>
    _value

  fun demo(): U32 =>
    this.get()
