class \nodoc\ _Leaf
  let value: U64

  new create(value': U64) => value = value'

class \nodoc\ val _Parent
  // The leaf is reachable only through the parent.
  let leaf: _Leaf val
  // When a message carries a val, the runtime traces the val's fields only if
  // its type can reference an actor. This field lets _Parent reference one.
  let actor_field: (_AliasSender | None) = None

  new val create(value: U64) => leaf = recover val _Leaf(value) end

class \nodoc\ val _PlainParent
  // Like _Parent, but its type can't reference an actor. A message carrying
  // this val marks it immutable instead of tracing its fields, and the
  // sender keeps the leaf alive by tracing the immutable parent itself.
  let leaf: _Leaf val

  new val create(value: U64) => leaf = recover val _Leaf(value) end

actor \nodoc\ _AliasSender
  embed _scratch: _Scratch = _Scratch
  let _receiver: _AliasReceiver = _AliasReceiver
  let _leaves: Array[_Leaf] = Array[_Leaf]

  new create() => _send()

  be _send() =>
    let tag_first = _Parent(0x6691)
    let val_first = _Parent(0x6692)
    let plain_tag_first = _PlainParent(0x6693)
    let plain_val_first = _PlainParent(0x6694)
    // Between each pair of messages, each alias is traced first in one of
    // them, whichever order a message traces its arguments in.
    _receiver.tag_first(tag_first, tag_first)
    _receiver.val_first(val_first, val_first)
    _receiver.plain_tag_first(plain_tag_first, plain_tag_first)
    _receiver.plain_val_first(plain_val_first, plain_val_first)
    _churn()

  be _churn() =>
    // This actor no longer holds any parent, so only the receiver's
    // references keep the leaves alive. Collect, then fill freed leaf-sized
    // slots with other values.
    let reused = _scratch.reuses_storage()
    if not reused then @pony_exitcode(34) end
    var i: USize = 0
    while i < 4096 do
      _leaves.push(_Leaf(0))
      i = i + 1
    end
    _receiver.check()

actor \nodoc\ _AliasReceiver
  embed _scratch: _Scratch = _Scratch
  let _parents: Array[_Parent] = Array[_Parent]
  let _plain: Array[_PlainParent] = Array[_PlainParent]

  be tag_first(t: _Parent tag, v: _Parent val) =>
    _parents.push(v)
    let reused = _scratch.reuses_storage()
    if not reused then @pony_exitcode(35) end

  be val_first(v: _Parent val, t: _Parent tag) =>
    _parents.push(v)
    let reused = _scratch.reuses_storage()
    if not reused then @pony_exitcode(36) end

  be plain_tag_first(t: _PlainParent tag, v: _PlainParent val) =>
    _plain.push(v)

  be plain_val_first(v: _PlainParent val, t: _PlainParent tag) =>
    _plain.push(v)

  be check() =>
    let reused = _scratch.reuses_storage()
    if not reused then @pony_exitcode(37) end
    try
      if _parents(0)?.leaf.value != 0x6691 then @pony_exitcode(38) end
      if _parents(1)?.leaf.value != 0x6692 then @pony_exitcode(39) end
      if _plain(0)?.leaf.value != 0x6693 then @pony_exitcode(41) end
      if _plain(1)?.leaf.value != 0x6694 then @pony_exitcode(42) end
    else
      @pony_exitcode(40)
    end
