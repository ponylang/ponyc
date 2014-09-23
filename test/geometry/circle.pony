class Circle
  var _radius: F32

  new create(radius': F32) =>
    _radius = radius'

  fun ref get_radius(): F32 =>
    _radius

  fun ref get_area(): F32 =>
    F32.pi() * _radius.pow(2)

  fun ref get_circumference(): F32 =>
    2 * _radius * F32.pi()

actor Main
  new create(env: Env) =>
    var c: Circle

    for i in Range[F32](1.0, 100.0) do
      c = Circle(i)
      @printf[I32]("Radius:%f\nCircumference: %f\nArea: %f\n".cstring(),
        c.get_radius().f64(), c.get_circumference().f64(), c.get_area().f64())
    end
