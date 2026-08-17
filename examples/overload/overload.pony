"""
This is an example of Pony's built-in backpressure. You'll note that if this
program was run with fair scheduling of all actors that the single `Receiver`
instance would be unable to keep up with all the `Sender` instances sending
messages to the `Receiver` instance. The result would be runaway memory
growth as the mailbox for `Receiver` grew larger and larger.

Thanks to Pony's built-in backpressure mechanism, this doesn't happen. As the
`Receiver` instance becomes overloaded, the Pony runtime responds by not
scheduling the various `Sender` instances until the overload on `Receiver` is
cleared.
"""
