trait T
  fun foo(): Stringable
  fun bar(): String => foo().string()
  fun baz(): String => this.foo().string()
