"""
This example is a stress test of Pony messaging and mailbox usage.
All X Mailers send to a single actor, Main, attempting to overload its
mailbox. This is a degenerate condition, the more actors you have sending
to Main (and the more messages they send), the more memory will be used.

Run this and watch the process memory usage grow as actor Main gets swamped.

Also note, it finishes reasonably quickly for how many messages that single
actor ends up processing.
"""
