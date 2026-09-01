use @pony_exitcode[None](code: I32)

class val Foo
class val Bar

type FooBar is (Foo | Bar)

type Nums is (F64 | I64)

type Nums3 is (F64 | I64 | U32)

primitive Helper
  fun test_class[J: FooBar val](): J ? =>
    iftype J <: Foo val then recover Foo end
    elseif J <: Bar val then recover Bar end
    else error
    end

  fun test_num[J: Nums val](): J ? =>
    iftype J <: F64 val then F64(42)
    elseif J <: I64 val then I64(42)
    else error
    end

  fun test_return[J: FooBar val](): J ? =>
    iftype J <: Foo val then return recover Foo end
    elseif J <: Bar val then return recover Bar end
    end
    error

  fun test_return_num[J: Nums val](): J ? =>
    iftype J <: F64 val then return F64(99)
    elseif J <: I64 val then return I64(99)
    end
    error

  fun test_nested[J: FooBar val](): J ? =>
    iftype J <: FooBar val then
      iftype J <: Foo val then recover Foo end
      elseif J <: Bar val then recover Bar end
      else error
      end
    else error
    end

  fun test_nested_return[J: FooBar val](): J ? =>
    iftype J <: FooBar val then
      iftype J <: Foo val then return recover Foo end
      elseif J <: Bar val then return recover Bar end
      end
    end
    error

  fun test_three_nums[J: Nums3 val](): J ? =>
    iftype J <: F64 val then F64(1)
    elseif J <: I64 val then I64(2)
    elseif J <: U32 val then U32(3)
    else error
    end

actor Main
  new create(env: Env) =>
    try
      let f: Foo = Helper.test_class[Foo]()?
      let b: Bar = Helper.test_class[Bar]()?
      let x: F64 = Helper.test_num[F64]()?
      let y: I64 = Helper.test_num[I64]()?
      let f2: Foo = Helper.test_return[Foo]()?
      let b2: Bar = Helper.test_return[Bar]()?
      let x2: F64 = Helper.test_return_num[F64]()?
      let y2: I64 = Helper.test_return_num[I64]()?
      let f3: Foo = Helper.test_nested[Foo]()?
      let b3: Bar = Helper.test_nested[Bar]()?
      let f4: Foo = Helper.test_nested_return[Foo]()?
      let b4: Bar = Helper.test_nested_return[Bar]()?
      let n1: F64 = Helper.test_three_nums[F64]()?
      let n2: I64 = Helper.test_three_nums[I64]()?
      let n3: U32 = Helper.test_three_nums[U32]()?
      @pony_exitcode(1)
    end
