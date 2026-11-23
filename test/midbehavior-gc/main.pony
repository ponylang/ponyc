use "collections"
use "runtime_info"
use @pony_exitcode[None](code: I32)
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])

use @retain_alias[None](p: Array[U64] tag)
use @take_alias[Array[U64]]()
use @final_count[U32]()
use @count_final[None]()

class \nodoc\ _Sentinel
  fun _final() => @count_final()

struct \nodoc\ _NullFields
  let ptr: Pointer[U8] = Pointer[U8]
  let nullable: NullablePointer[_NullFields] =
    NullablePointer[_NullFields].none()

actor \nodoc\ Main
  var _buffer: Array[U64] = Array[U64]
  let _check_final: Bool
  let _actor_stats: ActorStatsAuth
  let _scheduler_stats: SchedulerStatsAuth

  new create(env: Env) =>
    _actor_stats = ActorStatsAuth(env.root)
    _scheduler_stats = SchedulerStatsAuth(env.root)
    let mode = try env.args(1)? else "" end
    _check_final = mode == "check-finalizers"
    match mode
    | "transfer" => _Owner
    | "alias-order" => _AliasSender
    else
      _churn(43, true)
    end

  fun _save(seed: U64) =>
    @retain_alias(Array[U64].init(seed, 1))

  be _churn(seed: U64, collecting: Bool) =>
    if collecting then
      @pony_triggergc(@pony_ctx())
      if _check_final then _Sentinel end
    elseif _check_final and (@final_count() != 1) then
      @pony_exitcode(10)
    end
    let saved = Array[U64].init(seed, 1)
    if collecting then
      try @pony_exitcode(saved(1)?.i32()) end
    end
    let count = ActorStats.heap_gc_counter(_actor_stats)
    let mark_cpu = SchedulerStats.actor_gc_mark_cpu(_scheduler_stats)
    let result = _tuple(seed, collecting)
    ifdef runtimestats then
      if collecting and (ActorStats.heap_gc_counter(_actor_stats) <= count) then
        @pony_exitcode(21)
      end
      if collecting and
        (SchedulerStats.actor_gc_mark_cpu(_scheduler_stats) <= mark_cpu)
      then
        @pony_exitcode(22)
      end
    end
    try
      if (result._7 != saved(0)?) or (result._1 != (seed + 1)) then
        @pony_exitcode(7)
      end
    else
      @pony_exitcode(8)
    end

  fun \noinline\ ref _tuple(seed: U64, collecting: Bool):
    (U64, U64, U64, U64, U64, U64, U64)
  =>
    _descend(5, seed, collecting)
    (seed + 1, seed + 2, seed + 3, seed + 4, seed + 5, seed + 6, seed)

  fun \noinline\ ref _descend(depth: U64, seed: U64, collecting: Bool) =>
    let saved = Array[U64].init(seed + depth, 2)
    if depth > 0 then
      let visit: _Visit = if (depth % 2) == 0 then _First else _Second end
      visit(this, depth - 1, seed, collecting)
    else
      _allocate(seed, collecting)
    end
    try
      if saved(1)? != (seed + depth) then @pony_exitcode(5) end
    else
      @pony_exitcode(6)
    end

  fun \noinline\ ref _allocate(seed: U64, collecting: Bool) =>
    if not collecting then _save(seed) end
    if not _null_fields() then @pony_exitcode(23) end
    let live = Array[U64].init(seed, 1)
    let identities: Array[USize] iso = recover iso Array[USize] end
    var reused = false
    for i in Range[U64].till(0, 128) do
      _buffer = Array[U64].init(i, 8192)
      let id = digestof _buffer
      if identities.contains(id) then reused = true end
      identities.push(id)
    end
    let recorded: Array[USize] val = consume identities
    try
      if recorded.size() != 128 then
        @pony_exitcode(9)
      elseif (live(0)? != seed) or (_buffer(8191)? != 127) then
        @pony_exitcode(2)
      elseif reused != collecting then
        @pony_exitcode(1)
      elseif collecting then
        if _check_final and (@final_count() != 0) then @pony_exitcode(11) end
        _churn(seed + 1, false)
      elseif @take_alias()(0)? != seed then
        @pony_exitcode(4)
      end
    else
      @pony_exitcode(3)
    end

  fun \noinline\ _null_fields(): Bool =>
    let fields = _NullFields
    fields.ptr.is_null() and fields.nullable.is_none()
