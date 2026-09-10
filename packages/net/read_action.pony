primitive KeepReading
  """
  Returned from `_on_received` to let the read loop take the next message.
  """

primitive YieldReading
  """
  Returned from `_on_received` to stop the read loop after this message, giving
  other actors a turn. Reading resumes on its own in the next scheduler turn.

  Unlike `mute()`, which stops reading until `unmute()` reverses it, this is a
  one-shot pause.
  """

type ReadAction is (KeepReading | YieldReading)
  """
  What the read loop should do once `_on_received` returns.
  """
