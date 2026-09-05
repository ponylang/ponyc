use @pony_exitcode[None](code: I32)

class MyBox[A]
  new create() => None

  fun clone(): MyBox[A]^ =>
    iftype A <: Any val then
      recover iso MyBox[A].create() end
    else
      MyBox[A].create()
    end

  fun clone_share(): MyBox[A]^ =>
    iftype A <: Any #share then
      recover iso MyBox[A].create() end
    else
      MyBox[A].create()
    end

actor Main
  new create(env: Env) =>
    let a = MyBox[String val].create()
    a.clone()
    a.clone_share()
    let b = MyBox[U32].create()
    b.clone()
    b.clone_share()
    let c = MyBox[String ref].create()
    c.clone()
    c.clone_share()
    @pony_exitcode(1)
