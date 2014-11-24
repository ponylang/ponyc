use "files"
use "options"
use "collections"

actor Worker
  new mandelbrot(main: Main, x: U64, y: U64, width: U64, iterations: U64,
    limit: F32, real: Array[F32] val, imaginary: Array[F32] val)
    =>
    var view: Array[U8] iso =
      recover
        Array[U8].prealloc((y - x) * (width >> 3))
      end

    let group_r = Array[F32].undefined(8)
    let group_i = Array[F32].undefined(8)

    var row = x

    while row < y do
      let prefetch_i = imaginary.unsafe_get(row)

      var col: U64 = 0

      while col < width do
        var j: U64 = 0

        while j < 8 do
          group_r.unsafe_set(j, real.unsafe_get(col + j))
          group_i.unsafe_set(j, prefetch_i)
          j = j + 1
        end

        var bitmap: U8 = 0xFF
        var n = iterations

        repeat
          var mask: U8 = 0x80
          var k: U64 = 0

          while k < 8 do
            let r = group_r.unsafe_get(k)
            let i = group_i.unsafe_get(k)

            group_r.unsafe_set(k, ((r * r) - (i * i)) +
              real.unsafe_get(col + k))
            group_i.unsafe_set(k, (2.0 * r * i) + prefetch_i)

            if ((r * r) + (i * i)) > limit then
              bitmap = bitmap and not mask
            end

            mask = mask >> 1
            k = k + 1
          end
        until (bitmap == 0) or ((n = n - 1) == 1) end

        view.append(bitmap)

        col = col + 8
      end
      row = row + 1
    end

    main.draw(x * (width >> 3), consume view)

actor Main
  var iterations: U64 = 50
  var limit: F32 = 4.0
  var chunks: U64 = 16
  var width: U64 = 16000
  var actors: U64 = 0
  var header: U64 = 0
  var real: Array[F32] val
  var imaginary: Array[F32] val
  var outfile: (File | None) = None

  new create(env: Env) =>
    arguments(env)

    let length = width
    let recip_width = 2.0 / width.f32()

    var r = recover Array[F32].prealloc(length) end
    var i = recover Array[F32].prealloc(length) end

    for j in Range(0, width) do
      r.append((recip_width * j.f32()) - 1.5)
      i.append((recip_width * j.f32()) - 1.0)
    end

    real = consume r
    imaginary = consume i

    spawn_actors()
    create_outfile()

  be draw(offset: U64, pixels: Array[U8] val) =>
    match outfile
    | var out: File =>
      out.seek_start(header + offset)
      out.write(pixels)
      if (actors = actors - 1) == 1 then
        out.close()
      end
    end

  fun ref create_outfile() =>
    match outfile
    | var f: File =>
      f.print("P4\n " + width.string() + " " + width.string() + "\n")
      header = f.size()
      f.set_length((width * (width >> 3)) + header)
    end

  fun ref spawn_actors() =>
    actors = ((width + (chunks - 1)) / chunks)

    var rest = width % chunks

    if rest == 0 then rest = chunks end

    var x: U64 = 0
    var y: U64 = 0

    for i in Range(0, actors - 1) do
      x = i * chunks
      y = x + chunks
      Worker.mandelbrot(this, x, y, width, iterations, limit, real, imaginary)
    end

    Worker.mandelbrot(this, y, y + rest, width, iterations, limit, real,
      imaginary)

  fun ref arguments(env: Env) =>
    let options = Options(env)

    options
      .add("iterations", "i", None, I64Argument)
      .add("limit", "l", None, F64Argument)
      .add("chunks", "c", None, I64Argument)
      .add("width", "w", None, I64Argument)
      .add("output", "o", None, StringArgument)

    for option in options do
      match option
      | ("iterations", var arg: I64) => iterations = arg.u64()
      | ("limit", var arg: F64) => limit = arg.f32()
      | ("chunks", var arg: I64) => chunks = arg.u64()
      | ("width", var arg: I64) => width = arg.u64()
      | ("output", var arg: String) => outfile = try File(arg) end
      | ParseError => usage(env)
      end
    end

  fun tag usage(env: Env) =>
    env.out.print(
      """
      mandelbrot [OPTIONS]

      The binary output can be converted to a BMP with the following command
      (ImageMagick Tools required):

        convert <output> JPEG:<output>.jpg

      Available options:

      --iterations, -i  Maximum amount of iterations to be done for each pixel.
                        Defaults to 50.

      --limit, -l       Square of the limit that pixels need to exceed in order
                        to escape from the Mandelbrot set.
                        Defaults to 4.0.

      --chunks, -c      Maximum line count of chunks the image should be
                        divided into for divide & conquer processing.
                        Defaults to 16.

      --width, -w       Lateral length of the resulting mandelbrot image.
                        Defaults to 16000.

      --output, -o      File to write the output to.

      """
      )
