"""
# Backpressure Package

The Backpressure package allows Pony programmers to participate in Pony's
runtime backpressure system. The goal of the backpressure system is to prevent
an actor's mailbox from growing at an unbounded rate.

At a high level, the runtime backpressure system works by adjusting the
scheduling of actors. When an actor becomes overload, the Pony runtime will
deprioritize scheduling the actors that are sending to it. This change in
scheduling allows the overloaded actor to catch up.

The Pony runtime can detect overloading based on message queue size. However,
the overloading of some types of actors is harder to detect. Let's take the
case of actors like `TCPConnection`.

`TCPConnection` manages a socket for sending data to and receiving data from
another process. TCP connections can experience backpressure from the outside
our Pony program that prevents them from sending. There's no way for the Pony
runtime to detect this so, intervention by the programmer is needed.

`TCPConnection` is a single example. This Backpressure package exists to allow
a programmer to indicate to the runtime that a given actor is experiencing
pressure and sending messages to it should be adjusted accordingly.

Any actor that needs to be able to tell the runtime to "send me messages
slower" due to external conditions can do so via this package. Additionally,
actors that maintain there own internal queues of any sort, say for buffering,
are also prime candidates to for using this package. If an actor's internal
queue grows too large, it can call `Backpressure.apply` to let the runtime know
it is under pressure.

## Example program

// Here we have a TCPConnectionNotify that upon construction
// is given a BackpressureAuth token. This allows the notifier
// to inform the Pony runtime when to apply and release backpressure
// as the connection experiences it.
// Note the calls to
//
// Backpressure.apply(_auth)
// Backpressure.release(_auth)
//
// that apply and release backpressure as needed

use "backpressure"
use "collections"
use "net"

class SlowDown is TCPConnectionNotify
  let _auth: BackpressureAuth
  let _out: StdStream

  new iso create(auth: BackpressureAuth, out: StdStream) =>
    _auth = auth
    _out = out

  fun ref throttled(connection: TCPConnection ref) =>
    _out.print("Experiencing backpressure!")
    Backpressure.apply(_auth)

  fun ref unthrottled(connection: TCPConnection ref) =>
    _out.print("Releasing backpressure!")
    Backpressure.release(_auth)

  fun ref connect_failed(conn: TCPConnection ref) =>
    None

actor Main
  new create(env: Env) =>
    try
      let auth = env.root as AmbientAuth
      let socket = TCPConnection(auth, recover SlowDown(auth, env.out) end,
        "", "7669")
    end

## Caveat

The runtime backpressure is a powerful system. By intervening, programmers can
create deadlocks. Any call to `Backpressure.apply` should be matched by a
corresponding call to `Backpressure.release`. Authorization via the
`ApplyReleaseBackpressureAuth` capability is required to apply or release
backpressure. By requiring that the caller have a token to apply or release a
backpressure, rouge 3rd party library code can't run wild and unknowingly
mess with the runtime.
"""

use @pony_apply_backpressure[None]()
use @pony_release_backpressure[None]()

type BackpressureAuth is (AmbientAuth | ApplyReleaseBackpressureAuth)

primitive Backpressure
  fun apply(auth: BackpressureAuth) =>
    @pony_apply_backpressure()

  fun release(auth: BackpressureAuth) =>
    @pony_release_backpressure()
