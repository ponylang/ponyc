class Circle
  let _pi: F32 = 3.14
  var _radius: F32
  var _circ: F32

  new create(radius': F32) =>
    _radius = radius'
    _circ = 2.0 * _radius

  fun ref get_radius(): F32 =>
    _radius

  fun ref get_area(): F32 =>
    _radius * _radius * _pi

  fun ref get_circumference(): F32 =>
    _circ * _pi

actor Main
  new create(env: Env) =>
    var c: Circle

    for i in Range[F32](1.0, 100000.0) do
      c = Circle(i)
      @printf[I32]("Radius:%f\nCircumference: %f\nArea: %f\n"._cstring(),
        c.get_radius(), c.get_circumference(), c.get_area())
    end
