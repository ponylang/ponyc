class A
  let n: USize
  let m: USize

  new create(n': USize) =>
    n = n'
    m = 0

  new with_m(n': USize, m': USize) =>
    n = n'
    m = m'

actor Main
  new create(env: Env) =>
    // Test partial application with all arguments provided
    let ctor1 = A~create(1)
    let a1: A = ctor1()

    // Test partial application of constructor with multiple params
    let ctor2 = A~with_m(2, 3)
    let a2: A = ctor2()

    // Test partial application with only some arguments
    let ctor3 = A~with_m(4)
    let a3: A = ctor3(5)

    // Test using partial application result multiple times
    let ctor4 = A~create(6)
    let a4a: A = ctor4()
    let a4b: A = ctor4()

    // Verify all values are correct
    if (a1.n == 1) and (a2.n == 2) and (a2.m == 3) and
       (a3.n == 4) and (a3.m == 5) and (a4a.n == 6) and (a4b.n == 6) then
      return
    else
      env.exitcode(1)
    end
