use c = "collections"
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val =>
    object val is T[Array[A] val]
      fun n(): String =>
        let f = {(): USize => c.List[USize].>push(4).size() }
        "n" + f().string()
    end
