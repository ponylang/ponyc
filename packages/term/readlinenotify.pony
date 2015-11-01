use "promises"

interface ReadlineNotify
  """
  Notifier for readline.
  """
  fun ref apply(line: String, prompt: Promise[String]) =>
    """
    Receives finished lines. The returned string is the next prompt. This may
    be called with an empty line to get the current prompt. If this raises an
    error, readline will stop handling input.
    """
    None

  fun ref tab(line: String): Seq[String] box =>
    """
    Return tab completion possibilities.
    """
    Array[String]
