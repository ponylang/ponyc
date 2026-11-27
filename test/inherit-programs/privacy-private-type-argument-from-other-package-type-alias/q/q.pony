use l = "../l"

primitive _Mine
  fun name(): String => "q's private Mine"

type Fooable is l.T[_Mine]

primitive Make
  fun mine(): HasMine => _Mine

type HasMine is l.HasName val
