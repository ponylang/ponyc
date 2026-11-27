interface HasY
  fun y(): String
interface Getter
  fun get(): HasY val
trait T[A: Getter val]
  fun bar(a: A): String => a.get().y()
