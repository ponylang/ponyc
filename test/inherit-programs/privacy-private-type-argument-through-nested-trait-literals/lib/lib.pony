trait T[X: Stringable val]
  fun x(): X
  fun s(): String => x().string()
  fun a(v: X): T[X] val => object val is T[X] fun x(): X => v end
  fun b(v: X): T[X] val => object val is T[X] fun x(): X => v end
