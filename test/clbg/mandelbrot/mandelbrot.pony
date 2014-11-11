use "files"

actor Worker
  var _main: Main
  var _from: U64
  var _to: U64
  var _size: U64
  var _next: Worker
  var _coordinator: Bool = false
  var _complex: Array[(F32, F32)] val

  new create(main: Main, from: U64, to: U64, size: U64, next: Worker,
    clx: Array[(F32, F32)] val)
    =>
    _main = main
    _from = from
    _to = to
    _size = size
    _next = next
    _complex = clx

  new spawn_ring(main: Main, size: U64, chunk_size: U64) =>
    let actors = ((size + (chunk_size - 1)) / chunk_size) - 1
    var rest = size % chunk_size

    if rest == 0 then rest = chunk_size end

    var i: U64 = 0

    var clx: Array[(F32, F32)] iso =
      recover Array[(F32, F32)].prealloc(size) end

    while i < size do
      let real = ((F32(2.0)/size.f32())*i.f32()) - 1.5
      let imag = ((F32(2.0)/size.f32())*i.f32()) - 1.0

      clx.append((real, imag))

      i = i + 1
    end

    _complex = consume clx

    var n: Worker = this

    var j: U64 = 0
    var x: U64 = 0
    var y: U64 = 0

    while j < actors do
      x = j * chunk_size
      y = x + chunk_size
      n = Worker(main, x, y, size, n, _complex)
      j = j + 1
    end

    _main = main
    _from = y
    _to = y + rest
    _size = size
    _coordinator = true
    _next = n

  be mandelbrot(limit: F32, iterations: U64) =>
    _next.pixels(limit, iterations)

  be pixels(limit: F32, iterations: U64) =>
    if not _coordinator then
      _next.pixels(limit, iterations)
    else
      _next.done()
    end

    var y = _from

    let to = _to
    let size = _size

    var view: Array[(U64, U8)] iso =
      recover
        Array[(U64, U8)].prealloc((to - y) * size)
      end

    let group = Array[(F32, F32)].undefined(8)

    try
      while y < _to do
        let prefetch_i = _complex(y)._2

        var x: U64 = 0

        while x < _size do
          for i in Range[U64](0, 8) do
            group.update(i, (_complex(x + i)._1, prefetch_i))
          end

          var bitmap: U8 = 0xFF
          var n = iterations

          repeat
            var mask: U8 = 0x80

            for j in Range[U64](0, 8) do
              let c = group(j)
              let r = c._1
              let i = c._2

              let new_r = ((r * r) - (i * i)) + _complex(x + j)._1
              let new_i = (2.0*r*i) + prefetch_i

              group.update(j, (new_r, new_i))

              if ((r * r) + (i * i)) > limit then
                bitmap = bitmap and not mask
              end

              mask = mask >> 1
            end
          until (bitmap == 0) or ((n = n - 1) == 1) end

          view.append(((y * (_size >> 3)) + (x >> 3), bitmap))
          x = x + 8
        end
        y = y + 1
      end
    end

    _main.draw(consume view)

  be done() =>
    if _coordinator then
      _main.dump()
    else
      _next.done()
    end

actor Main
  let _env: Env
  var _square_limit: F32 = 4.0
  var _iterations: U64 = 50
  var _lateral_length: U64 = 16000
  var _chunk_size: U64 = 16
  var _image: Array[U8]
  var _outfile: (File | None)

  new create(env: Env) =>
    _env = env
    _image = Array[U8] //init tracking
    _outfile = None

    try
      arguments()

      _image = Array[U8].undefined(_lateral_length * (_lateral_length >> 3))

      Worker
        .spawn_ring(this, _lateral_length, _chunk_size)
        .mandelbrot(_square_limit, _iterations)
    else
      usage()
    end

  be draw(pixels: Array[(U64, U8)] val) =>
    try
      for bitmap in pixels.values() do
        _image.update(bitmap._1, bitmap._2)
      end
    end

  be dump() =>
    let x: String val = _lateral_length.string()

    match _outfile
    | var o: File => o.print("P4\n"+ x + " " + x + "\n") ; o.write(_image)
    end

  fun ref arguments() ? =>
    var n: U64 = 1

    while n < _env.args.length() do
      var option = _env.args(n)
      var value = _env.args(n + 1)
      n = n + 2

      match option
      | "--limit" => _square_limit = value.f32()
      | "--iterations" => _iterations = value.u64()
      | "--lateral_length" => _lateral_length = ((value.u64() + 7)/8)*8
      | "--chunk_size" => _chunk_size = ((value.u64() + 7)/8)*8
      | "--output" => _outfile = File(value)
      else
        error
      end

      if _chunk_size > _lateral_length then error end
    end

  fun ref usage() =>
    _env.stdout.print(
      """
      mandelbrot [OPTIONS]

      The output is written to stdout. The binary output can be converted to
      a BMP with the following command (ImageMagick Tools required):

        convert <output> -background black -alpha remove -alpha off -colors 16
                         -compress none BMP2:<output>.bmp

      Available options:

      --limit           Square of the limit that pixels need to exceed in order
                        to escape from the Mandelbrot set.
                        Defaults to 4.0.

      --iterations      Maximum amount of iterations to be done for each pixel.
                        Defaults to 50.

      --lateral_length  Lateral length of the resulting mandelbrot image.
                        Defaults to 16000.

      --chunk_size      Maximum line count of chunks the image should be
                        divided into for divide & conquer processing.
                        Defaults to 16.

      --output          File to write the output to.
      """
      )
