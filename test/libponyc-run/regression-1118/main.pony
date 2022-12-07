use "collections"

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

    for i in Range(0, 500_000) do
      t.do_it()
    end

  fun @runtime_override_defaults(rto: RuntimeOptions) =>
     rto.ponynoblock = true

actor Test
  be do_it() =>
    _BoomActor.dispatch(Payload)
