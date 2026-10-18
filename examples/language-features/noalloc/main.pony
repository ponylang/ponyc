actor Main
  new create(env: Env) =>
    let simple: _Scorer = _SimpleScorer
    let weighted: _Scorer = _WeightedScorer(3)

    env.out.print("Simple: " + simple.score(10, 20).string())
    env.out.print("Weighted: " + weighted.score(10, 20).string())
