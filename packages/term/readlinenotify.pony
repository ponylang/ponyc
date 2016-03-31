use "promises"

interface ReadlineNotify
  """
  Notifier for readline.
  """
  fun ref apply(line: String, prompt: Promise[String]) =>
    """
    Receives finished lines. The next prompt is set by fulfilling the promise.
    If the promise is rejected, readline will stop handling input.
    """
    None

  fun ref tab(line: String): Seq[String] box =>
    """
    Return tab completion possibilities.
    """
    Array[String]
