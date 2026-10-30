type TracingCategory is
  ( SchedulerTracing
  | SchedulerMessagingTracing
  | ActorTracing
  | ActorBehaviorTracing
  | ActorGCTracing
  | ActorStateChangeTracing
  | SystematicTestingTracing
  | SystematicTestingDetailsTracing
  | RuntimeLifecycleTracing
  | SchedulerStealingTracing
  | CycleDetectorTracing )

primitive SchedulerTracing
  """
  Scheduler thread lifecycle and state events.
  """
  fun apply(): U64 => 1

primitive SchedulerMessagingTracing
  """
  Inter-scheduler message send and receive events.
  """
  fun apply(): U64 => 2

primitive ActorTracing
  """
  Actor creation and destruction events.
  """
  fun apply(): U64 => 4

primitive ActorBehaviorTracing
  """
  Actor behavior scheduling and execution events.
  """
  fun apply(): U64 => 8

primitive ActorGCTracing
  """
  Actor garbage collection phase events.
  """
  fun apply(): U64 => 16

primitive ActorStateChangeTracing
  """
  Actor state transitions: muted, overloaded, blocked, etc.
  """
  fun apply(): U64 => 32

primitive SystematicTestingTracing
  """
  Systematic testing lifecycle events.
  """
  fun apply(): U64 => 64

primitive SystematicTestingDetailsTracing
  """
  Detailed systematic testing events (timeslices, waiting).
  """
  fun apply(): U64 => 128

primitive RuntimeLifecycleTracing
  """
  Runtime init, start, and shutdown events.
  """
  fun apply(): U64 => 256

primitive SchedulerStealingTracing
  """
  Work-stealing success and failure events.
  """
  fun apply(): U64 => 512

primitive CycleDetectorTracing
  """
  Actor events for the cycle detector only. Enables all actor event types
  (creation, destruction, behavior runs, GC phases, state changes) but only
  for the cycle detector actor.
  """
  fun apply(): U64 => 1024

