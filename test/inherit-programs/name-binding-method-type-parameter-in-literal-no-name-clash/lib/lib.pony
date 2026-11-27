trait T
  fun m[B: Stringable val](b: B): Stringable val =>
    object val is Stringable
      fun string(): String iso^ => recover String .> append(b.string()) end
    end
