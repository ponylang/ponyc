use @pony_exitcode[None](code: I32)

actor Main
  new create(e: Env) =>
    var src: Array[U8] = [1]
    var dest: Array[U8] = [11; 1; 2; 3; 4; 5; 6]
    src.copy_to(dest, 11, 0, 10)
    try
      let v = dest(0)?.i32()
      @pony_exitcode(v)
    end
