use "files"
use "options"

actor Worker
  new mandelbrot(main: Main, x: U64, y: U64, width: U64, iterations: U64,
    limit: F32, complex: Array[(F32, F32)] val)
    =>
    var view: Array[U8] iso =
      recover
        Array[U8].prealloc((y - x) * (width >> 3))
      end

    let group = Array[(F32, F32)].undefined(8)

    var from = x

    try
      while from < y do
        let prefetch_i = complex(from)._2

        var col: U64 = 0

        while col < width do
          for j in Range(0, 8) do
            group.update(j, (complex(col + j)._1, prefetch_i))
          end

          var bitmap: U8 = 0xFF
          var n = iterations

          repeat
            var mask: U8 = 0x80

            for k in Range(0, 8) do
              let c = group(k)
              let r = c._1
              let i = c._2

              let new_r = ((r * r) - (i * i)) + complex(col + k)._1
              let new_i = (2.0*r*i) + prefetch_i

              group.update(k, (new_r, new_i))

              if ((r * r) + (i * i)) > limit then
                bitmap = bitmap and not mask
              end

              mask = mask >> 1
            end
          until (bitmap == 0) or ((n = n - 1) == 1) end

          view.append(bitmap)

          col = col + 8
        end
        from = from + 1
      end
    end

    main.draw(x * (width >> 3), consume view)

actor Main
  var iterations: U64 = 50
  var limit: F32 = 4.0
  var chunks: U64 = 16
  var width: U64 = 16000
  var actors: U64 = 0
  var header: U64 = 0
  var complex: Array[(F32, F32)] val
  var outfile: (File | None) = None

  new create(env: Env) =>
    arguments(env)

    let length = width
    var c = recover Array[(F32, F32)].prealloc(length) end

    for j in Range(0, width) do
      let r = ((F32(2.0)/width.f32())*j.f32()) - 1.5
      let i = ((F32(2.0)/width.f32())*j.f32()) - 1.0
      c.append((r, i))
    end

    complex = consume c

    create_outfile()
    spawn_actors()

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
      header = f.length()
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
      Worker.mandelbrot(this, x, y, width, iterations, limit, complex)
    end

    Worker.mandelbrot(this, y, y + rest, width, iterations, limit, complex)

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
    env.stdout.print(
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
