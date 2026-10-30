type ActorTracingMode is
  ( ActorTracingNone
  | ActorTracingAll
  | ActorTracingCycleDetectorOnly )

primitive ActorTracingNone
  """
  No actors emit actor-level trace events.
  """
  fun apply(): U8 => 0

primitive ActorTracingAll
  """
  All actors emit actor-level trace events.
  """
  fun apply(): U8 => 1

primitive ActorTracingCycleDetectorOnly
  """
  Only the cycle detector emits actor-level trace events.
  """
  fun apply(): U8 => 2
