// Regression test for issue #4281: Incorrect array inference
// When a union type like (Bar box | Baz box | Bool) is used as an element type
// of ReadSeq, the array inference should correctly strip the this-> viewpoint
// from union members when checking if array elements are subtypes.

type Foo is (Bar box | Baz box | Bool)

class Bar
  embed _items: Array[Foo] = _items.create()

  new create(items: ReadSeq[Foo]) =>
    for item in items.values() do
      _items.push(item)
    end

class Baz

actor Main
  new create(env: Env) =>
    let bar = Bar([
      true
      Bar([ false ])
    ])
