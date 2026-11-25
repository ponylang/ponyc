trait Picker
  fun pick[Res: Any val](r: Res): Res => r

class Holder[Res: Any val] is Picker

actor Main
  new create(env: Env) => None
