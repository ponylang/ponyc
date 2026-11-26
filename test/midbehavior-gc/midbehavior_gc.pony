"""
Checks that collection during a behavior reuses dead storage while keeping
what the behavior can still reach: arrays held by suspended callers
(including callers in a recursion with no loop, as it descends and as it
unwinds), an array held only as the value a recursive call returns, objects
with finalizers, objects sent to or received from other actors, and the
children of a shared `val` that a message also carries as a `tag`.
"""
