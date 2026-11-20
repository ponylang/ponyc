"""
Exercise root aliases, a consumed parameter, and a pending finalizer.
"""

use @pony_exitcode[None](code: I32)
use @puts[I32](s: Pointer[U8] tag)

class \nodoc\ val Leaf
  let n: U64

  new val create(n': U64) => n = n'

class \nodoc\ val Resource
  fun _final() => @puts("resource finalized".cstring())

class \nodoc\ Parcel
  var n: U64 = 0

actor \nodoc\ Sink
  var parcel: (Parcel | None) = None

  be take(item: Parcel iso) =>
    item.n = 17
    parcel = consume item

actor \nodoc\ Worker
  new create() =>
    let a = Leaf(7)
    audit_roots(this, a, a, 11)
    audit_transfer(recover iso Parcel end, Sink)

  be audit_roots(peer: Worker tag, a: Leaf val, b: Leaf tag, n: U64) =>
    let x: Leaf val = Leaf(n)
    let y: Leaf val = x
    if (peer is this) and (a is b) and (x is y) and (x.n == n) then
      @pony_exitcode(0)
    end
    audit_tuple((a, a))
    with_resource()

  be audit_tuple(pair: (Leaf val, Leaf val)) => None

  be audit_transfer(item: Parcel iso, sink: Sink) =>
    sink.take(consume item)
    @puts("sent".cstring())

  be with_resource() =>
    let resource: Resource val = Resource
    audit_finalisers()
    hold(resource)

  be audit_finalisers() => None

  be hold(resource: Resource) => None

actor \nodoc\ Main
  new create(env: Env) =>
    @pony_exitcode(1)
    Worker
