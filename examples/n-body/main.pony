actor Main
  let system: Array[Body] = Array[Body](5)
  let sun: Body = Body.sun()
  let _env: Env

  new create(env: Env) =>
    _env = env

    let n =
      try
        env.args(1)?.usize()?
      else
        50000000
      end

    system
      .> push(sun)
      .> push(Body.jupiter())
      .> push(Body.saturn())
      .> push(Body.uranus())
      .> push(Body.neptune())

    offset_momentum()
    print_energy()

    var i: USize = 0

    while i < n do
      advance(0.01)
      i = i + 1
    end

    print_energy()

  fun ref advance(dt: F64) =>
    """
    Advance the simulation by one time step.
    """
    let count = system.size()
    var i: USize = 0

    while i < count do
      try
        let body = system(i)?
        var j = i + 1

        while j < count do
          body.attract(system(j)?, dt)
          j = j + 1
        end
      end

      i = i + 1
    end

    try
      i = 0

      while i < system.size() do
        let body = system(i)?
        body.integrate(dt)
        i = i + 1
      end
    end

  fun ref print_energy() =>
    _env.out.print(energy().string())

  fun ref energy(): F64 =>
    """
    Calculate the total energy of the system.
    """
    let count = system.size()
    var e: F64 = 0
    var i: USize = 0

    while i < count do
      try
        let body = system(i)?
        e = e + body.ke()
        var j = i + 1

        while j < count do
          e = e - body.pe(system(j)?)
          j = j + 1
        end
      end

      i = i + 1
    end
    e

  fun ref offset_momentum() =>
    """
    Adjust the sun's velocity so total system momentum is zero.
    """
    var px: F64 = 0
    var py: F64 = 0
    var pz: F64 = 0

    try
      var i: USize = 0

      while i < system.size() do
        var body = system(i)?
        px = px - (body.vx * body.mass)
        py = py - (body.vy * body.mass)
        pz = pz - (body.vz * body.mass)
        i = i + 1
      end
    end

    sun.vx = px / sun.mass
    sun.vy = py / sun.mass
    sun.vz = pz / sun.mass
