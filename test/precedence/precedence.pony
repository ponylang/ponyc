use "../collections"

trait Test
trait Test2

class Precedence[A: (Test), B: (Test|List[A]), C: {fun mut(): Array[B]?},
  D:(Test|List[A])] is List[B], Test
  /* nested
  /* comments */
  work */
  fun imm assoc(): F32 => 1 + 2 * 3 + 4

  fun box assoc2(): I32 => 1 + 77 * 2.0e-2 * 0xFF / 0b11 % 012 + 3.hello()

  fun mut logic(): Bool => a and b or c xor d

  fun tag compare(): Bool => a < b > c == d != e <= f >= g

  fun trn optarg() =>
    obj.apply(arg where opt = if 3 > 4 then "te\nst" else "real" end)

trait Dormouse[A: B, B: A]

trait Fooable[A: Fooable[A]]

class Wombat is Fooable[Wombat]

class Wallaby[A: Dormouse[Test, Test]]

class Aardvark[A]

class Foo[T]
  var a: Aardvark[T] mut

  fun box get(): this.Aardvark[T] mut => a
  fun mut set(a': Aardvark[T] mut): this.Aardvark[T] mut^ => a = a'

type FooOrAardvark[A]: (Aardvark[A] | Foo[A])

type SomeWallaby[A: Dormouse[Test, Test]]: (Wallaby[A] | Aardvark[A])

type OtherFooable[A: Fooable[A]]: Fooable[A]

trait Zero

trait One is Zero

trait Two is One

trait First[A: Zero]

trait Second[A: One] is First[A]

trait Third[A: Two] is Second[A]

trait Fourth[A: First[_]]

class Fifth is Third[Two]
  var a: Fourth[Fifth]

  fun box foo[A: Fooable[A]](a': A): this.A => a

class Functor[A: {fun box(I32, (String | None)): Bool}]

class Sophia[X: Fooable[X]] is Fooable[X]

class Subject[A: Observer[A, B], B: Subject[A, B]]

class Observer[A: Observer[A, B], B: Subject[A, B]]
