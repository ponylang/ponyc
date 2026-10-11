trait Describable
  fun describe(): String

interface Countable
  fun count(): USize

class Collector is Describable
  let _name: String val
  var _items: Array[String] ref

  new create(name: String val) =>
    _name = name
    _items = Array[String]

  fun ref add(item: String) =>
    _items.push(item)

  fun box size(): USize =>
    _items.size()

  fun describe(): String =>
    _name + ": " + _items.size().string() + " items"

class IsoHolder
  var _data: Array[U8] iso

  new create() =>
    _data = recover iso Array[U8] end

  fun ref set_data(data: Array[U8] iso) =>
    _data = consume data

primitive Helpers
  fun make_val_string(): String val =>
    recover val
      let s = String
      s.append("hello")
      s
    end

  fun do_recover(): String iso^ =>
    recover iso
      let x: USize = 42
      x.string()
    end

actor Worker
  let _name: String val
  var _count: USize

  new create(name: String val) =>
    _name = name
    _count = 0

  be work(item: String val) =>
    _count = _count + 1

class GenericBox[A: Any val]
  let _value: A

  new create(value: A) =>
    _value = value

  fun get(): A =>
    _value

actor Main
  new create(env: Env) =>
    let collector = Collector("test")
    collector.add("one")
    let s = collector.size()
    let desc = collector.describe()

    let worker = Worker("w1")
    worker.work("task")

    let box_str = GenericBox[String]("hello")

    let helpers_result = Helpers.make_val_string()
    let iso_string = Helpers.do_recover()

class CapMethods
  var _data: USize

  new create() =>
    _data = 0

  new from_value(v: USize) =>
    _data = v

  fun box read_only(): USize =>
    _data

  fun ref mutate(v: USize) =>
    _data = v

  fun val immutable_op(): String =>
    _data.string()

  fun tag identity(): None =>
    None

  fun box partial_lookup(key: USize): USize ? =>
    if key == 0 then error end
    _data + key

  fun box with_default(a: USize, b: USize = 10): USize =>
    a + b + _data

trait Steppable
  fun step(): USize

class SteppableCollector is (Describable & Steppable)
  let _name: String val
  var _pos: USize

  new create(name: String val) =>
    _name = name
    _pos = 0

  fun describe(): String =>
    _name

  fun step(): USize =>
    _pos

trait Greetable
  fun greet(): String => "hello"
  fun farewell(): String => "goodbye"

class GreetableUser is Greetable
  new create() => None

  fun greet(): String => "hi"
  // farewell() is NOT overridden — inherited from Greetable

class _PrivateHelper
  new create() => None
  fun helper(): String => "private"

type StringOrNone is (String | None)

class DocumentedType
  """
  A type with a docstring for testing exports.
  """
  new create() => None

struct ExportStruct
  new create() => None

class ScopeFixture
  let _val_field: String val
  var _count: USize

  new create() =>
    _val_field = "field"
    _count = 0

  fun ref scope_test(): String iso^ =>
    let outer_val: String val = "hello"
    let outer_ref: String ref = "world".clone()
    recover iso
      let inner_local: USize = 42
      inner_local.string()
    end

primitive UnionProducer
  fun get_value(flag: Bool): (Collector | SteppableCollector) =>
    if flag then Collector("union") else SteppableCollector("union") end

class TypeApiFixture
  fun ref use_types(env: Env) =>
    let union_val = UnionProducer.get_value(true)
    union_val.describe()
    let c: CapMethods val = CapMethods.from_value(42)
    env.out.print(c.immutable_op())
    let divergent = DivergentProducer.get(true)
    divergent.shared()
    let vp_holder = ViewpointHolder[String]("test")
    let vp_result = vp_holder.get()

class CountableItem is Countable
  var _n: USize

  new create(n: USize) =>
    _n = n

  fun count(): USize =>
    _n

class StructuralCountable
  var _n: USize

  new create(n: USize) =>
    _n = n

  fun count(): USize =>
    _n

primitive StructuralPrimitive
  fun count(): USize =>
    99

class DivergentA
  new create() => None
  fun box shared(): USize => 0
  fun box info(): String => "a"

class DivergentB
  new create() => None
  fun box shared(): USize => 0
  fun box info(): USize => 0

primitive DivergentProducer
  fun get(flag: Bool): (DivergentA | DivergentB) =>
    if flag then DivergentA else DivergentB end

class ViewpointHolder[A]
  let _data: A

  new create(data: A) =>
    _data = consume data

  fun get(): this->A =>
    _data
