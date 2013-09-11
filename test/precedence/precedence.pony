use "../collections"

trait Test
trait Test2

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

trait Dormouse[A:B, B:A]

trait Fooable[A:Fooable[A]]

class Wombat is Fooable[Wombat]

class Wallaby[A:Dormouse[Test, Test]]

class Aardvark[A]

class Foo[T]
  var a:Aardvark[T]{var}

  fun get{iso|val|var}():Aardvark[T]{var}->this = a
  fun set{var}(a':Aardvark[T]{var}):Aardvark[T]{var} = a = a'

alias FooOrAardvark[A]:(Aardvark[A]|Foo[A])

alias SomeWallaby[A:Dormouse[Test, Test]]:(Wallaby[A]|Aardvark[A])

alias OtherFooable[A:Fooable[A]]:Fooable[A]

trait Zero

trait One is Zero

trait Two is One

trait First[A:Zero]

trait Second[A:One] is First[A]

trait Third[A:Two] is Second[A]

trait Fourth[A:First[_]]

class Fifth is Third[Two]
  var a:Fourth[Fifth]

  fun foo[A:Fooable[A]](a':A):A = a
