trait T
  fun m[B: Stringable val](b: B): Stringable val =>
    let s: String val = b.string()
    object val is Stringable
      fun string(): String iso^ => s.clone()
    end
