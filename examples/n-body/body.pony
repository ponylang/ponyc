class Body
  """
  A body in the simulation with position, velocity, and mass.
  """
  var x: F64
  var y: F64
  var z: F64
  var vx: F64
  var vy: F64
  var vz: F64
  var mass: F64

  new sun() =>
    """
    Create the sun at the origin.
    """
    x = 0; y = 0; z = 0
    vx = 0; vy = 0; vz = 0
    mass = 1
    _init()

  new jupiter() =>
    """
    Create Jupiter with its known orbital parameters.
    """
    x = 4.8414314424647209
    y = -F64(1.16032004402742839)
    z = -F64(1.03622044471123109e-1)

    vx = 1.66007664274403694e-3
    vy = 7.69901118419740425e-3
    vz = -F64(6.90460016972063023e-5)

    mass = 9.54791938424326609e-4
    _init()

  new saturn() =>
    """
    Create Saturn with its known orbital parameters.
    """
    x = 8.34336671824457987
    y = 4.12479856412430479
    z = -F64(4.03523417114321381e-1)

    vx = -F64(2.76742510726862411e-3)
    vy = 4.99852801234917238e-3
    vz = 2.30417297573763929e-5

    mass = 2.85885980666130812e-4
    _init()

  new uranus() =>
    """
    Create Uranus with its known orbital parameters.
    """
    x = 1.28943695621391310e1
    y = -F64(1.51111514016986312e1)
    z = -F64(2.23307578892655734e-1)

    vx = 2.96460137564761618e-3
    vy = 2.37847173959480950e-3
    vz = -F64(2.96589568540237556e-5)

    mass = 4.36624404335156298e-5
    _init()

  new neptune() =>
    """
    Create Neptune with its known orbital parameters.
    """
    x = 1.53796971148509165e1
    y = -F64(2.59193146099879641e1)
    z = 1.79258772950371181e-1

    vx = 2.68067772490389322e-3
    vy = 1.62824170038242295e-3
    vz = -F64(9.51592254519715870e-5)

    mass = 5.15138902046611451e-5
    _init()

  fun ref attract(that: Body, dt: F64) =>
    """
    Calculate gravitational attraction with another body and update
    both bodies' velocities.
    """
    let dx = x - that.x
    let dy = y - that.y
    let dz = z - that.z
    let d = ((dx * dx) + (dy * dy) + (dz * dz)).sqrt()
    let mag = dt / (d * d * d)

    vx = vx - (dx * mag * that.mass)
    vy = vy - (dy * mag * that.mass)
    vz = vz - (dz * mag * that.mass)

    that.vx = that.vx + (dx * mag * mass)
    that.vy = that.vy + (dy * mag * mass)
    that.vz = that.vz + (dz * mag * mass)

  // Integrate new position.
  fun ref integrate(dt: F64) =>
    x = x + (vx * dt)
    y = y + (vy * dt)
    z = z + (vz * dt)

  // Kinetic energy: 0.5 m v^2
  fun ke(): F64 => mass * ((vx * vx) + (vy * vy) + (vz * vz)) * 0.5

  fun pe(that: Body box): F64 =>
    """
    Potential energy between this body and another.
    """
    let dx = x - that.x
    let dy = y - that.y
    let dz = z - that.z
    let d = ((dx * dx) + (dy * dy) + (dz * dz)).sqrt()
    (mass * that.mass) / d

  fun ref _init() =>
    vx = vx * _days_per_year()
    vy = vy * _days_per_year()
    vz = vz * _days_per_year()
    mass = mass * _solar_mass()

  fun tag _solar_mass(): F64 => F64.pi() * F64.pi() * 4
  fun tag _days_per_year(): F64 => 365.24

