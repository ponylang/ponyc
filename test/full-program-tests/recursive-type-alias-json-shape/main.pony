// End-to-end test for the canonical JSON-shape recursive type alias
// — the flagship use case enabled by Phase 3 of recursive type aliases
// (ponylang/ponyc#5007). Uses Map (which the libponyc test fixture
// can't load), so this exercises the runtime trace and reach paths
// for an alias whose SCC reaches itself through both Array's typearg
// and Map's typearg.

use "collections"
use @pony_exitcode[None](code: I32)

type _JsonNull    is None
type _JsonBoolean is Bool
type _JsonString  is String
type _JsonNumber  is F64
type _JsonArray   is Array[_JsonEntity]
type _JsonObject  is Map[_JsonString, _JsonEntity]

type _JsonEntity is
  ( _JsonNull
  | _JsonBoolean
  | _JsonString
  | _JsonNumber
  | _JsonArray
  | _JsonObject)

actor Main
  new create(env: Env) =>
    let arr: _JsonArray = _JsonArray
    arr.push(F64(3.14))
    arr.push("hello")
    arr.push(true)
    arr.push(None)

    let inner: _JsonArray = _JsonArray
    inner.push(F64(1))
    inner.push(F64(2))
    arr.push(inner)

    let obj: _JsonObject = _JsonObject
    obj("count") = F64(arr.size().f64())
    obj("nested") = arr
    arr.push(obj)

    @pony_exitcode(arr.size().i32())
