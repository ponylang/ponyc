interface Maker
  fun make(): Stringable
trait T[A: Maker val]
  fun bar(a: A): String => a.make().string()
