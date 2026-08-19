use @printf[I32](fmt: Pointer[U8] tag, ...)
use @exit[None](status: I32)

actor Main
  new create(env: Env) =>
    let nodes: USize = 48
    let tokens: USize = 600
    let ttl: USize = 24
    let collector = Collector(tokens)
    let mesh': Array[Node] iso = recover Array[Node](nodes) end
    var i: USize = 0
    while i < nodes do
      mesh'.push(Node(collector, i, nodes))
      i = i + 1
    end
    let mesh: Array[Node] val = consume mesh'
    for node in mesh.values() do
      node.wire(mesh)
    end
    // Wire every node before injecting tokens; causal messaging then delivers
    // each node's wire() ahead of any ping() that can reach it, so _mesh is
    // always set when ping() runs.
    var t: USize = 0
    while t < tokens do
      try mesh(t % nodes)?.ping("seed-" + t.string(), ttl, t) end
      t = t + 1
    end

actor Node
  """
  Forwards tokens on a spreading route and allocates a fresh
  `String val` payload each hop, so receiving nodes accumulate
  references to payloads owned by many distinct senders.
  """
  let _collector: Collector
  let _id: USize
  let _n: USize
  var _mesh: Array[Node] val = recover val Array[Node] end

  new create(collector: Collector, id: USize, n: USize) =>
    _collector = collector
    _id = id
    _n = n

  be wire(mesh: Array[Node] val) =>
    _mesh = mesh

  be ping(payload: String val, hops: USize, id: USize) =>
    if hops > 0 then
      // Spread the route across many neighbors so each node
      // receives payloads owned by many distinct nodes (->
      // multi-owner foreign GC map). `next` is always in
      // [0, _n), so the indexed access cannot error. The
      // incoming payload is dropped and a fresh one (owned by
      // this node) is forwarded, so the owner of each in-flight
      // payload is its immediate sender.
      let next = (_id + hops) % _n
      let fresh: String val = "p-" + id.string() + "-" + hops.string()
      try _mesh(next)?.ping(fresh, hops - 1, id) end
    else
      _collector.recv(id, _id)
    end

actor Collector
  """
  Folds every arrival's (token_id, node_id) into an order-sensitive
  FNV-1a hash and prints the resulting ORDER_SIG when all tokens
  have arrived.
  """
  let _expected: USize
  var _received: USize = 0
  // FNV-1a-style rolling hash of the arrival order: an interleaving signature
  var _sig: U64 = 14695981039346656037

  new create(expected: USize) =>
    _expected = expected

  be recv(token_id: USize, node_id: USize) =>
    """
    Record one arrival and, when all have arrived, print the
    ORDER_SIG and exit.
    """
    _mix(token_id.u64())
    _mix(node_id.u64())
    _received = _received + 1
    if _received == _expected then
      // Synchronous printf then forced exit, as in the other fixtures:
      // env.out.print is async and would be lost on @exit, and the forced exit
      // sidesteps the separate shutdown-hang symptom. The signature is fully
      // computed first, so the determinism result is unaffected.
      let line = "RECEIVED=" + _received.string()
        + " ORDER_SIG=" + _sig.string() + "\n"
      @printf("%s".cstring(), line.cstring())
      @exit(0)
    end

  fun ref _mix(v: U64) =>
    _sig = (_sig xor v) * 1099511628211
