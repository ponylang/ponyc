use "collections"
use debug = "debug"

actor Main
  var name: String

  new create(env: Env) =>
    name =
      recover val
        String.create(10) .> append("snot")
      end
    env.out.print(name)

trait Snot
  fun bla(): U8

  fun blubb(): Bool => bla() == U8(0xFF)

class Foo is Snot
  """
  docstring
  """

  var variable: (Array[String] iso | None) = None
  embed s: String = String.create(0b10)
  let immutable: Map[String, USize] = Map[String, USize].create()
 
  fun bla(): U8 => F32(0.12345e-4).u8()

  fun with_default(arg: String tag, len: USize = -1) =>
    let obj = object
      let field: U8 = U64(12).u8()
      fun apply(arg: Array[String] = []): USize =>
        match arg.size()
        | let s: USize if s > 0 => s
        else
          // TODO: partial application is still a great mess
          let applied = USize~max_value()
          applied()
        end
    end


trait Res1
  fun res(): U8

trait Res2
  fun res(): U8

class X is (Res1 & Res2)
  let f: U8 = 0

  fun res(): U8 =>
    f

class Y is (Res1 & Res2)
  let f: U8 = 1

  fun res(): U8 =>
    f

class FieldTypes
  let tuple_field: (String, X, Y)
  let union_field: (X | Y)
  let isect_field: (Res1 & Res2)

  new create() =>
    tuple_field = ("", X, Y)
    union_field = Y
    isect_field = tuple_field._2
    let something1 = union_field.res()
    let something2 = isect_field.res()


trait WithEmptyConstructor
  new ref create()

class WithGenericParam[T: WithEmptyConstructor ref]
  let f: T

  new create() =>
    f = T.create()


primitive Log
  fun apply(msg: String) =>
    debug.Debug(msg)

type Union is (_Snot | _Badger)

primitive _Snot
primitive _Badger

class Matching
  fun do_a_match(u: Union) =>
    match u
    | _Snot =>
      do_snot()
    | _Badger =>
      do_badger()
    end

  fun do_snot(): String =>
    "snot"

  fun do_badger(): String =>
    "badger"
