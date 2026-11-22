use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])
use @record_final[None](id: U32)
use @final_count[U32](id: U32)

class _Child
  let id: U32
  new create(id': U32) => id = id'
  fun _final() => @record_final(id)

class _Parent
  let child: _Child
  new iso create(id: U32) => child = _Child(id)

actor _Keeper
  var _shared: (_Parent val | None)
  var _isolated: (_Parent iso | None)

  new create(owner: Main, shared: _Parent val, isolated: _Parent iso) =>
    _shared = shared
    _isolated = consume isolated
    @pony_triggergc(@pony_ctx())
    _held(owner)

  be _held(owner: Main) => owner.check_held(this)

  be drop(owner: Main) =>
    _shared = None
    _isolated = None
    @pony_triggergc(@pony_ctx())
    _dropped(owner)

  be _dropped(owner: Main) => owner.check_dropped()

actor Main
  let _env: Env
  let _shared_tag: _Parent tag
  var _shared: (_Parent val | None)
  var _isolated: (_Parent iso | None)
  let _isolated_tag: _Parent tag

  new create(env: Env) =>
    _env = env
    let shared: _Parent val = _Parent(1)
    let isolated = _Parent(2)
    _shared_tag = shared
    _shared = shared
    _isolated_tag = isolated
    _isolated = consume isolated
    _collect()
    _transfer()

  fun _collect() =>
    // This unreachable object's finaliser witnesses collection on its owner.
    _Child(0)
    @pony_triggergc(@pony_ctx())

  fun _check(stage: String, collections: U32, children: U32): Bool =>
    let controls = @final_count(0)
    let shared = @final_count(1)
    let isolated = @final_count(2)
    if (controls == collections) and (shared == children) and
      (isolated == children)
    then
      true
    else
      _env.err.print(stage + ": finaliser counts: control=" + controls.string()
        + ", shared=" + shared.string() + ", isolated=" + isolated.string())
      false
    end

  be _transfer() =>
    if _check("before transfer", 1, 0) then
      let shared = _shared = None
      let isolated = _isolated = None
      match (shared, consume isolated)
      | (let s: _Parent val, let i: _Parent iso) =>
        _Keeper(this, s, consume i)
      end
    end

  be check_held(keeper: _Keeper) =>
    _collect()
    _after_collection(keeper)

  be _after_collection(keeper: _Keeper) =>
    if _check("held by keeper", 2, 0) then keeper.drop(this) end

  be check_dropped() =>
    _collect()
    _finish()

  be _finish() =>
    if _shared_tag is _isolated_tag then
      _env.err.print("Expected distinct parents")
    elseif _check("dropped by keeper", 3, 1) then
      _env.exitcode(1)
    end
