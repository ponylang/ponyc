class Body
  var x: F64
  var y: F64
  var z: F64

  //var pad: F64 = F64(0)

  var vx: F64
  var vy: F64
  var vz: F64

  var mass: F64

  new sun() =>
    x = 0; y = 0; z = 0
    vx = 0; vy = 0; vz = 0
    mass = 1
    _init()

  new jupiter() =>
    x = 4.8414314424647209
    y = -F64(1.16032004402742839)
    z = -F64(1.03622044471123109e-1)

    vx = 1.66007664274403694e-3
    vy = 7.69901118419740425e-3
    vz = -F64(6.90460016972063023e-5)

    mass = 9.54791938424326609e-4
    _init()

  new saturn() =>
    x = 8.34336671824457987
    y = 4.12479856412430479
    z = -F64(4.03523417114321381e-1)

    vx = -F64(2.76742510726862411e-3)
    vy = 4.99852801234917238e-3
    vz = 2.30417297573763929e-5

    mass = 2.85885980666130812e-4
    _init()

  new uranus() =>
    x = 1.28943695621391310e1
    y = -F64(1.51111514016986312e1)
    z = -F64(2.23307578892655734e-1)

    vx = 2.96460137564761618e-3
    vy = 2.37847173959480950e-3
    vz = -F64(2.96589568540237556e-5)

    mass = 4.36624404335156298e-5
    _init()

  new neptune() =>
    x = 1.53796971148509165e1
    y = -F64(2.59193146099879641e1)
    z = 1.79258772950371181e-1

    vx = 2.68067772490389322e-3
    vy = 1.62824170038242295e-3
    vz = -F64(9.51592254519715870e-5)

    mass = 5.15138902046611451e-5
    _init()

  // Calculate attraction with another body.
  fun ref attract(that: Body, dt: F64) =>
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
  fun box ke(): F64 => mass * ((vx * vx) + (vy * vy) + (vz * vz)) * 0.5

  // Potential energy: m0 m1 / d
  fun box pe(that: Body box): F64 =>
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

actor Main
  let system: Array[Body] = Array[Body].prealloc(5)
  let sun: Body = Body.sun()
  let _env: Env

  new create(env: Env) =>
    _env = env

    let n = try
      env.args(1).u64()
    else
      U64(50000000)
    end

    // Initial system
    system
      .append(sun)
      .append(Body.jupiter())
      .append(Body.saturn())
      .append(Body.uranus())
      .append(Body.neptune())

    offset_momentum()
    print_energy()

    var i: U64 = 0

    while i < n do
      advance(0.01)
      i = i + 1
    end

    print_energy()

  fun ref advance(dt: F64) =>
    let count = system.size()
    var i: U64 = 0

    while i < count do
      try
        let body = system(i)
        var j = i + 1

        while j < count do
          // TODO: appears to be faster when not inlined
          body.attract(system(j), dt)
          //let body2 = system(j)
          //let dx = body.x - body2.x
          //let dy = body.y - body2.y
          //let dz = body.z - body2.z
          //let d = ((dx * dx) + (dy * dy) + (dz * dz)).sqrt()
          //let mag = dt / (d * d * d)
          //
          //body.vx = body.vx - (dx * mag * body2.mass)
          //body.vy = body.vy - (dy * mag * body2.mass)
          //body.vz = body.vz - (dz * mag * body2.mass)
          //
          //body2.vx = body2.vx + (dx * mag * body.mass)
          //body2.vy = body2.vy + (dy * mag * body.mass)
          //body2.vz = body2.vz + (dz * mag * body.mass)
          j = j + 1
        end
      end

      i = i + 1
    end

    try
      i = 0

      while i < system.size() do
        let body = system(i)
        body.integrate(dt)
        //body.x = body.x + (body.vx * dt)
        //body.y = body.y + (body.vy * dt)
        //body.z = body.z + (body.vz * dt)
        i = i + 1
      end
    end

  fun ref print_energy() =>
    _env.stdout.print(energy().string())

  fun ref energy(): F64 =>
    let count = system.size()
    var e: F64 = 0
    var i: U64 = 0

    while i < count do
      try
        let body = system(i)
        e = e + body.ke()
        var j = i + 1

        while j < count do
          e = e - body.pe(system(j))
          j = j + 1
        end
      end

      i = i + 1
    end
    e

  fun ref offset_momentum() =>
    var px: F64 = 0
    var py: F64 = 0
    var pz: F64 = 0

    try
      var i: U64 = 0

      while i < system.size() do
        var body = system(i)
        px = px - (body.vx * body.mass)
        py = py - (body.vy * body.mass)
        pz = pz - (body.vz * body.mass)
        i = i + 1
      end
    end

    sun.vx = px / sun.mass
    sun.vy = py / sun.mass
    sun.vz = pz / sun.mass
