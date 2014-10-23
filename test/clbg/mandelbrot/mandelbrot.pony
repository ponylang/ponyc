actor Worker
  var _main: Main
  var _start: U64
  var _finish: U64
  var _size: U64
  var _length: U64
  var _initial_real: Array[F32]
  var _initial_imaginary: Array[F32]

  new create(main: Main, start: U64, finish: U64, length: U64, iter: U64,
    limit: F32)
    =>
    _main = main
    _start = start
    _finish = finish
    _size = finish - start
    _length = length

    _initial_real = Array[F32].prealloc(_size)
    _initial_imaginary = Array[F32].prealloc(_size)

    var i: U64 = start

    while i < finish do
      _initial_real.append(((F32(2.0)/_length.f32())*i.f32()) - 1.5)
      _initial_imaginary.append(((F32(2.0)/_length.f32())*i.f32()) - 1.0)
      i = i + 1
    end

    try
      escape_time(iter, limit)
      main.done()
    else
      main.fail()
    end

  fun ref escape_time(iterations: U64, limit: F32) ? =>
    let _group_real: Array[F32] = Array[F32].init(0, 8)
    let _group_imaginary: Array[F32] = Array[F32].init(0, 8)
    var y: U64 = _start

    while y < _finish do
      let prefetch_i = _initial_imaginary(y - _start)
      var k: U64 = 0

      while k < _size do
        var j: U64 = 0

        while j < 8 do
          _group_real.update(j, _initial_real(k + j))
          _group_imaginary.update(j, prefetch_i)
          j = j + 1
        end

        var bitmap: U8 = 0xFF
        var n = iterations

        repeat
          var pixel_mask: U8 = 0x80
          var pixel: U64 = 0

          while pixel < 8 do
            let r: F32 = _group_real(pixel)
            let i: F32 = _group_imaginary(pixel)

            _group_real.update(pixel, ((r*r) - (i*i)) + _initial_real(k + pixel))
            _group_imaginary.update(pixel, (F32(2.0)*r*i) + prefetch_i)

            if ((r * r) + (i * i)) > limit then
              bitmap = bitmap and not pixel_mask
            end

            pixel_mask = pixel_mask >> 1
            pixel = pixel + 1
          end
          n = n - 1
        until (bitmap != 0) and (n > 0) end

        _main.draw((y*(_length/8)) + (k/8), bitmap)
        k = k + 8
      end
      y = y + 1
    end

actor Main
  let _env: Env
  var _square_limit: F32 = 4.0
  var _iterations: U64 = 50
  var _lateral_length: U64 = 16000
  var _chunk_length: U64 = 256
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

  be fail() =>
    _env.stdout.print("Failed computing mandelbrot image!")

  be done() =>
    _done = _done + 1

    if _done == _actors then
      _env.stdout.print("Dump image")
    end

  be draw(coord: U64, pixels: U8) =>
    try
      _image.update(coord, pixels)
    else
      fail()
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
      | "--lateral_length" => _lateral_length = (value.u64() + 7) and not 7
      | "--chunk_length" => _chunk_length = (value.u64() + 7) and not 7
      else
        error
      end

      if _chunk_length > _lateral_length then error end
    end

  fun ref mandelbrot() =>
    //TODO: issue: #58, otherwise problematic for large bitmaps
    _image = Array[U8].init(0, (_lateral_length * _lateral_length) >> 3)

    var rest = _lateral_length % _chunk_length
    let half = _chunk_length >> 0

    _actors =
      if rest > 0 then
        if rest > half then
          rest = rest - half
          (_lateral_length / _chunk_length) + 2
        else
          (_lateral_length / _chunk_length) + 1
        end
      else
        rest = _chunk_length
        _lateral_length / _chunk_length
      end

    var n: U64 = 0
    var x: U64 = 0
    var y: U64 = 0

    while n < (_actors - 1) do
      x = n * _chunk_length
      y = x + _chunk_length

      Worker(this, x, y, _lateral_length, _iterations, _square_limit)
      n = n + 1
    end

    Worker(this, y, y + rest, _lateral_length, _iterations, _square_limit)

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
                        to escape from the Mandelbrot set. Defaults to 4.0.

      --iterations      Maximum amount of iterations to be done for each pixel.
                        Defaults to 50.

      --lateral_length  Lateral length of the resulting mandelbrot image.
                        Defaults to 16000.

      --chunk_length    Maximum lateral length of chunks the image should be
                        divided into for divide & conquer processing.
                        Defaults to 256.
      """
      )
