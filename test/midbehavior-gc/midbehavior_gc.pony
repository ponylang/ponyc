"""
Checks that collection during a behavior reuses dead storage while keeping
what the behavior can still reach: arrays held by suspended callers, objects
with finalizers, objects sent to or received from other actors, and the
children of a shared `val` that a message also carries as a `tag`.
"""
