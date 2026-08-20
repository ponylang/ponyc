use "collections"
use "files"

actor Main
  let c: Config
  var outfile: (File | None) = None
  var actors: USize = 0
  var header: USize = 0
  var real: Array[F32] val = recover Array[F32] end
  var imaginary: Array[F32] val =
    recover Array[F32] end

  new create(env: Env) =>
    try
      c = Config(env)?
      outfile =
        match c.outpath
        | let fp: FilePath => File(fp)
        else
          None
        end

      let length = c.width
      let recip_width = 2.0 / c.width.f32()

      var r = recover Array[F32](length) end
      var i = recover Array[F32](length) end

      for j in Range(0, c.width) do
        r.push((recip_width * j.f32()) - 1.5)
        i.push((recip_width * j.f32()) - 1.0)
      end

      real = consume r
      imaginary = consume i

      spawn_actors()
      create_outfile()
    end
    c = Config.none()

  be draw(offset: USize, pixels: Array[U8] val) =>
    match outfile
    | let out: File =>
      out.seek_start(header + offset)
      out.write(pixels)
      if (actors = actors - 1) == 1 then
        out.dispose()
      end
    end

  fun ref create_outfile() =>
    match outfile
    | let f: File =>
      f.print(
        "P4\n " + c.width.string() +
          " " + c.width.string() + "\n")
      header = f.size()
      f.set_length(
        (c.width * (c.width >> 3)) + header)
    end

  fun ref spawn_actors() =>
    """
    Divides the image into chunks and spawns workers.
    """
    actors = ((c.width + (c.chunks - 1)) / c.chunks)
    var rest = c.width % c.chunks
    if rest == 0 then rest = c.chunks end

    var x: USize = 0
    var y: USize = 0

    for i in Range(0, actors - 1) do
      x = i * c.chunks
      y = x + c.chunks
      Worker.mandelbrot(
        this,
        x,
        y,
        c.width,
        c.iterations,
        c.limit,
        real,
        imaginary)
    end

    Worker.mandelbrot(
      this,
      y,
      y + rest,
      c.width,
      c.iterations,
      c.limit,
      real,
      imaginary)
