"""
Plots a Mandelbrot set using a divide-and-conquer algorithm with
multiple worker actors.
"""

use "cli"
use "collections"
use "files"

actor Worker
  """
  Computes a horizontal slice of the Mandelbrot set.
  """

  new mandelbrot(
    main: Main,
    x: USize,
    y: USize,
    width: USize,
    iterations: USize,
    limit: F32,
    real: Array[F32] val,
    imaginary: Array[F32] val)
  =>
    """
    Computes pixels for rows `x` to `y`.
    """
    var view: Array[U8] iso =
      recover
        Array[U8]((y - x) * (width >> 3))
      end

    let group_r = Array[F32] .> undefined(8)
    let group_i = Array[F32] .> undefined(8)

    var row = x

    try
      while row < y do
        let prefetch_i = imaginary(row)?

        var col: USize = 0

        while col < width do
          var j: USize = 0

          while j < 8 do
            group_r.update(j, real(col + j)?)?
            group_i.update(j, prefetch_i)?
            j = j + 1
          end

          var bitmap: U8 = 0xFF
          var n = iterations

          repeat
            var mask: U8 = 0x80
            var k: USize = 0

            while k < 8 do
              let r = group_r(k)?
              let i = group_i(k)?

              group_r.update(
                k,
                ((r * r) - (i * i)) + real(col + k)?)?
              group_i.update(
                k, (2.0 * r * i) + prefetch_i)?

              if ((r * r) + (i * i)) > limit then
                bitmap = bitmap and not mask
              end

              mask = mask >> 1
              k = k + 1
            end
          until (bitmap == 0) or ((n = n - 1) == 1) end

          view.push(bitmap)

          col = col + 8
        end
        row = row + 1
      end

      main.draw(x * (width >> 3), consume view)
    end

class val Config
  """
  Mandelbrot rendering parameters.
  """

  let iterations: USize
  let limit: F32
  let chunks: USize
  let width: USize
  let outpath: (FilePath | None)

  new val create(env: Env) ? =>
    let cs =
      CommandSpec.leaf(
        "run",
        """
        Plot a Mandelbrot set using a divide-and-conquer
        algorithm.

        The output can be viewed directly with a suitable
        Netpbm viewer, or converted to a PNG with the
        following command (ImageMagick Tools required):

          convert <output> PNG:<output>.png""",
        [
          OptionSpec.i64(
            "iterations",
            "Maximum amount of iterations to be "
              + "done for each pixel."
            where short' = 'i', default' = 50)
          OptionSpec.f64(
            "limit",
            "Square of the limit that pixels need "
              + "to exceed in order to escape from "
              + "the Mandelbrot set."
            where short' = 'l', default' = 4.0)
          OptionSpec.i64(
            "chunks",
            "Maximum line count of chunks the "
              + "image should be divided into for "
              + "divide & conquer processing."
            where short' = 'c', default' = 16)
          OptionSpec.i64(
            "width",
            "Lateral length of the resulting "
              + "mandelbrot image."
            where short' = 'w', default' = 16000)
          OptionSpec.string(
            "output",
            "File to write the output to."
            where short' = 'o', default' = "")
        ])? .> add_help()?
    let cmd =
      match \exhaustive\ CommandParser(cs).parse(
        env.args, env.vars)
      | let c: Command => c
      | let ch: CommandHelp =>
        ch.print_help(env.out)
        env.exitcode(0)
        error
      | let se: SyntaxError =>
        env.out.print(se.string())
        env.exitcode(1)
        error
      end
    iterations =
      cmd.option("iterations").i64().usize()
    limit = cmd.option("limit").f64().f32()
    chunks = cmd.option("chunks").i64().usize()
    width = cmd.option("width").i64().usize()
    outpath =
      FilePath(
        FileAuth(env.root),
        cmd.option("output").string())

  new val none() =>
    """
    Creates an empty configuration for error recovery.
    """
    iterations = 0
    limit = 0
    chunks = 0
    width = 0
    outpath = None

