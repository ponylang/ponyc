use "collections"
use @printf[I32](fmt: Pointer[U8] tag, ...)
use "time"

actor _BoomActor
  be dispatch(request: Payload) =>
    request.holder = _BoomActorHolder(this)
    boom_behavior(consume request, Payload)

  be boom_behavior(request: Payload val, response: Payload val) =>
    None

class val _BoomActorHolder
  let boom_actor: _BoomActor
  new val create(boom_actor': _BoomActor) => boom_actor = boom_actor'

class iso Payload
  var holder: (_BoomActorHolder | None) = None

actor Main
  new create(env: Env) =>
    let t = Test

    @printf("starting loop\n".cstring())
    for i in Range(0, 500_000) do
      t.do_it()
    end
    @printf("finished loop\n".cstring())

  fun @runtime_override_defaults(rto: RuntimeOptions) =>
     rto.ponynoblock = true

actor Test
  var c: U64 = 0

  be do_it() =>
    c = c + 1
    if (c % 10_000) == 0 then
      @printf("Boom actor dispatch number: %ld at %ld\n".cstring(), c, Time.seconds())
    end
    _BoomActor.dispatch(Payload)
