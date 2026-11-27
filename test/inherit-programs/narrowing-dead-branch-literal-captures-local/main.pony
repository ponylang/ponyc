// For C, the iftype branch whose object literal captures array a cannot run,
// and C overrides get(), the element type of a. Expects 7 from the live
// branch's literal.
interface Sizer
  fun apply(): USize
trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun m(): Sizer ref =>
    let a = [get()]
    iftype A <: String then
      object ref
        fun apply(): USize => a.size()
      end
    else
      object ref fun apply(): USize => 7 end
    end
class C is T[U8]
  new create() => None
  fun get(): U64 => 1
actor Main
  new create(env: Env) => env.out.print(C.m()().string())
