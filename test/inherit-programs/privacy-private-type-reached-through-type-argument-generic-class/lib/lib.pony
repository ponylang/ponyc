interface HasY
  fun y(): String
interface val Getter
  fun get(): HasY val
class G[A: Getter val]
  new create() => None
  fun bar(a: A): String => a.get().y()
