use "../collections"

trait Test

class Precedence[A:(Test), B:(Test|List[A]), C:fun throw():Array[B],
  D:(Test|List[A])] is List[B], Test
  /* nested
  /* comments */
  work */
  fun assoc():I32 = 1_ + 77 * 2.0e-2 * 0xF_F / 0b1_1 % 01_2

  fun assoc2():F32 = 1 * 2 + 3

  fun logic():Bool = a and b or c xor d

  fun optarg() =
    obj.invoke(arg, if 3 > 4 then "te\nst" else "real"->opt)

trait Fooable[A:Fooable[A]]

class Aardvark[A] is Fooable[This]
  var a:A

class Foo[T]
  var a:Aardvark[_]{var}

  fun get{iso|val|var}():Aardvark[T]{var}->this = a
  fun set{var}(a':Aardvark[T]{var}):Aardvark[T]{var} = a = a'
