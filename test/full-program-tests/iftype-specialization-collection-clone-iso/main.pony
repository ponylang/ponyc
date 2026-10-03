use "collections"
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let list: List[String] val = recover val
      List[String] .> push("a") .> push("b")
    end
    let list_clone: List[String] iso = list.clone()

    let set: HashSet[U32, HashEq[U32]] val = recover val
      HashSet[U32, HashEq[U32]] .> set(1) .> set(2)
    end
    let set_clone: HashSet[U32, HashEq[U32]] iso = set.clone()

    let map: HashMap[String, U32, HashEq[String]] val = recover val
      HashMap[String, U32, HashEq[String]] .> update("a", 1) .> update("b", 2)
    end
    let map_clone: HashMap[String, U32, HashEq[String]] iso = map.clone()

    Receiver.accept(consume list_clone, consume set_clone,
      consume map_clone)

actor Receiver
  be accept(
    l: List[String] iso,
    s: HashSet[U32, HashEq[U32]] iso,
    m: HashMap[String, U32, HashEq[String]] iso)
  =>
    @pony_exitcode(I32(1))
