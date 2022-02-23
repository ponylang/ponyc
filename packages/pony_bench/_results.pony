use "collections"

class val _Results
  let name: String
  let samples: Array[U64]
  let iterations: U64
  let overhead: Bool

  new val create(
    name': String,
    samples': Array[U64] iso,
    iterations': U64,
    overhead': Bool)
  =>
    name = name'
    samples = consume samples'
    iterations = iterations'
    overhead = overhead'
    Sort[Array[U64], U64](samples)

  fun raw_str(): String =>
    let str = recover String end
    str .> append(name) .> append(",")
    for n in samples.values() do
      let nspi = n / iterations
      str .> append(nspi.string()) .> append(",")
    end
    if samples.size() > 0 then try str.pop()? end end
    str

  fun sum(): U64 =>
    var sum': U64 = 0
    try
      for i in Range(0, samples.size()) do
        sum' = sum' + samples(i)?
      end
    end
    sum'

  fun mean(): F64 =>
    sum().f64() / samples.size().f64()

  fun median(): F64 =>
    try
      let len = samples.size()
      let i = len / 2
      if (len % 2) == 1 then
        samples(i)?.f64()
      else
        (let lo, let hi) = (samples(i)?, samples(i + 1)?)
        ((lo.f64() + hi.f64()) / 2).round()
      end
    else
      0
    end

  fun std_dev(): F64 =>
    // sample standard deviation
    if samples.size() < 2 then return 0 end
    try
      var sum_squares: F64 = 0
      for i in Range(0, samples.size()) do
        let n = samples(i)?.f64()
        sum_squares = sum_squares + (n * n)
      end
      let avg_squares = sum_squares / samples.size().f64()
      let mean' = mean()
      let mean_sq = mean' * mean'
      let len = samples.size().f64()
      ((len / (len - 1)) * (avg_squares - mean_sq)).sqrt()
    else
      0
    end
