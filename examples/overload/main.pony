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
use "collections"

actor Receiver
  var _msgs: U64 = 0
  let _out: StdStream

  new create(out: StdStream) =>
    _out = out
    _out.print("Single receiver started.")

  be receive() =>
    _msgs = _msgs + 1
    if ((_msgs % 50_000_000) == 0) then
      _out.print(_msgs.string() + " messages received.")
    end

actor Sender
  let _receiver: Receiver

  new create(receiver: Receiver) =>
    _receiver = receiver

  be fire() =>
    _receiver.receive()
    fire()

actor Main
  var _size: U32 = 10_000

  new create(env: Env) =>
    start_messaging(env.out)
    loop()

  be loop() => None
    loop()

  fun ref start_messaging(out: StdStream) =>
    let r = Receiver(out)
    out.print("Starting " + _size.string() + " senders.")
    for i in Range[U32](0, _size) do
      Sender(r).fire()
    end
