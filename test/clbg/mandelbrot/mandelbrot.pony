actor Worker
  var _coordinator: Bool = false
  var _next: Worker
  var _main: Main
  var _size: U64
  var _chunk: U64
  var _chunk_size: U64
  var _iter: U64
  var _limit: F32
  var _x: U64
  var _y: U64

  var _real: Array[F32] val
  var _imag: Array[F32] val
  var _initial: Array[(Array[F32] val, Array[F32] val)] val

  new create(main: Main, size: U64, chunk_size: U64, iter: U64, limit: F32) =>
    let actors = ((size + (chunk_size - 1)) / chunk_size) - 1;
    var rest = size % chunk_size

    if rest == 0 then rest = chunk_size end

    var i: U64 = 0
    var n: Worker = this

    var x: U64 = 0
    var y: U64 = 0

    while i <= actors do
      x = i * chunk_size
      y = x + chunk_size
      n = Worker.init(main, n, size, i, chunk_size, x, y, iter, limit)
      i = i + 1
    end

    _coordinator = true
    _next = n
    _main = main
    _size = size
    _chunk = actors + 1
    _chunk_size = chunk_size
    _iter = iter
    _limit = limit
    _x = y
    _y = y + rest

    prepare()

  new init(main: Main, next: Worker, size: U64, chunk: U64, chunk_size: U64,
    from: U64, to: U64, iter: U64, limit: F32)
    =>
    _next = next
    _main = main
    _size = size
    _chunk = chunk
    _chunk_size = chunk_size
    _iter = iter
    _limit = limit
    _x = from
    _y = to

    prepare()

  be accumulate(view: Array[(Array[F32] val, Array[F32] val)] iso) =>
    view.append((_real, _imag))

    if not _coordinator then
      _next.accumulate(consume view)
    else
      _initial = consume view
      _next.escape_time(_initial)
    end

  be escape_time(initial: Array[(Array[F32] val, Array[F32] val)] val) =>
    try
      if not _coordinator then
        _next.escape_time(initial)
      end

      let group_r = Array[F32].init(0, 8)
      let group_i = Array[F32].init(0, 8)
      var n = _x

      while n < _y do
        let prefetch_i = (_initial(_chunk)._2)(n/_chunk_size)
        var m: U64 = 0

        while m < _size do
          for i in Range[U64](0, 8) do
            let gr =
            group_r.update(i, (_initial(m/_chunk_size)._1)(i))
            group_i.update(i, prefetch_i)
          end

          var bitmap: U8 = 0xFF
          var iterations: U64 = _iter

          repeat
            iterations = iterations - 1

            var mask: U8 = 0x80

            for j in Range[U64](0, 8) do
              let r = group_r(j)
              let i = group_i(j)

              group_r.update(j, ((r*r) - (i*i)) + (_initial(m/_chunk_size)._1)(j))
              group_i.update(j, ((F32(2.0)*r*i) + prefetch_i))

              if ((r*r) + (i*i)) > _limit then
                bitmap = bitmap and not mask
              end

              mask = mask >> 1
            end
          until (bitmap == 0) or (iterations == 0) end

          _main.draw(((n * _size)/8) + (m/8), bitmap)
          m = m + 8
        end
        n = n + 1
      end
    end

    if _coordinator then
      _next.done()
    end

  be done() =>
    if _coordinator then
      _main.dump()
    else
      _next.done()
    end

  fun ref prepare() =>
    var real: Array[F32] iso = recover Array[F32].prealloc(_y - _x) end
    var imag: Array[F32] iso = recover Array[F32].prealloc(_y - _x) end

    var i: U64 = _x

    while i < _y do
      real.append(((F32(2.0)/_size.f32())*i.f32()) - 1.5)
      imag.append(((F32(2.0)/_size.f32())*i.f32()) - 1.0)
      i = i + 1
    end

    _real = consume real
    _imag = consume imag

    if _coordinator then
      var view: Array[(Array[F32] val, Array[F32] val)] iso =
        recover Array[(Array[F32] val, Array[F32] val)].prealloc(_chunk) end

      _next.accumulate(consume view)
    end

actor Main
  let _env: Env
  var _square_limit: F32 = 4.0
  var _iterations: U64 = 50
  var _lateral_length: U64 = 16000
  var _chunk_size: U64 = 16
  var _done: U64 = 0
  var _actors: U64
  var _image: Array[U8]

  new create(env: Env) =>
    _env = env

    try
      arguments()
      mandelbrot()
    else
      usage()
    end

  be dump() =>
    @fprintf[I32](I32(1), "P4\n%jd %jd\n".cstring(), _lateral_length,
      _lateral_length)

    @fwrite[U64](_image.carray(), (_lateral_length * _lateral_length)/8,
      I32(1), I32(1))

  be draw(coord: U64, pixels: U8) =>
    try _image.update(coord, pixels) end

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
      else
        error
      end

      if _chunk_size > _lateral_length then error end
    end

  fun ref mandelbrot() =>
    //TODO: issue: #58, otherwise problematic for large bitmaps
    _image = Array[U8].init(0, _lateral_length * (_lateral_length >> 3))
    Worker(this, _lateral_length, _chunk_size, _iterations, _square_limit)

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
      """
      )
