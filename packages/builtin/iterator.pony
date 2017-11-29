interface Iterator[A]
  """

  Iterators generate a series of values, one value at a time on each call to `next()`.

  An Iterator is considered exhausted, once its `has_next()` method returns `false`.
  Thus every call to `next()` should be preceeded with a call to `has_next()` to
  check for exhaustiveness.

  ## Usage

  Given the rules for using Iterators mentioned above, basic usage
  of an iterator looks like this:

  ```pony
  while iterator.has_next() do
    let elem = iterator.next()?
    // do something with elem
  end
  ```

  The `For`-loop provides a more concise way of iteration:

  ```pony
  for elem in iterator do
    // do something with elem
  end
  ```

  Iteration using `While` is more flexible as it allows to continue iterating if a call to `next()` errors.
  The `For`-loop does not allow this.

  ## Implementing Iterators

  Iterator implementations need to adhere to the following rules to be considered well-behaved:

  * If the Iterator is exhausted, `has_next()` needs to return `false`.
  * Once `has_next()` returned `false` it is not allowed to switch back to `true`
    (Unless the Iterator supports rewinding)
  * `has_next()` does not change its returned value if `next()` has not been called.
    That means, that between two calls to `next()` any number of calls to `has_next()`
    need to return the same value. (Unless the Iterator supports rewinding)
  * A call to `next()` erroring does not necessarily denote exhaustiveness.

  ### Example

  ```pony
  // Generates values from `from` to 0
  class ref Countdown is Iterator[USize]
    var _cur: USize
    var _has_next: Bool = true

    new ref create(from: USize) =>
      _cur = from

    fun ref has_next(): Bool =>
      _has_next

    fun ref next(): USize =>
      let elem = _cur = _cur - 1
      if elem == 0 then
        _has_next = false
      end
      elem
  ```

  """

  fun ref has_next(): Bool
    """
    Returns `true` if this Iterator is not yet exhausted.
    That means that a value returned from a subsequent call to `next()`
    is a valid part of this iterator.

    Returns `false` if this Iterator is exhausted.

    The behavior of `next()` after this function returned `false` is undefined,
    it might throw an error or return values which are not part of this Iterator.
    """

  fun ref next(): A ?
    """
    Generate the next value.

    This might error, which does not necessarily mean that the Iterator is exhausted.
    """
