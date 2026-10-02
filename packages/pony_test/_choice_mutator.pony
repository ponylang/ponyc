primitive _ChoiceMutator
  """
  Perturb a choice sequence for targeted testing. Each choice is
  independently selected for perturbation with probability 1/sqrt(n),
  keeping values within the original bounds.
  """
  fun apply(
    choices: Array[_Choice val] val,
    rnd: Randomness ref)
    : Array[_Choice val] val
  =>
    let n = choices.size()
    if n == 0 then return choices end

    let threshold: F64 = F64(1.0) / n.f64().sqrt()

    var result = recover iso Array[_Choice val](n) end
    try
      var i: USize = 0
      while i < n do
        let c = choices(i)?
        let mutated =
          if rnd.f64(0.0, 1.0) < threshold then
            _perturb(c, rnd)
          else
            c
          end
        result.push(mutated)
        i = i + 1
      end
    end

    consume result

  fun _perturb(c: _Choice val, rnd: Randomness ref): _Choice val =>
    match \exhaustive\ c
    | let ic: _IntChoice => _perturb_int(ic, rnd)
    | let fc: _FloatChoice => _perturb_float(fc, rnd)
    | let bc: _BoolChoice => _perturb_bool(bc)
    | let uc: _U128Choice => _perturb_u128(uc, rnd)
    end

  fun _perturb_int(ic: _IntChoice, rnd: Randomness ref): _Choice val =>
    if ic.min == ic.max then return ic end
    let range = ic.max - ic.min
    let delta = range / 4
    if delta == 0 then
      let v = rnd._raw_int(ic.min, ic.max)
      return _IntChoice(v, ic.min, ic.max, ic.shrink_towards)
    end
    let offset = rnd._raw_int(-delta, delta)
    let raw = ic.value + offset
    let v = raw.max(ic.min).min(ic.max)
    _IntChoice(v, ic.min, ic.max, ic.shrink_towards)

  fun _perturb_float(fc: _FloatChoice, rnd: Randomness ref): _Choice val =>
    if fc.min == fc.max then return fc end
    let range = fc.max - fc.min
    let delta = range / 4.0
    if delta == 0.0 then return fc end
    let offset = rnd.f64(-delta, delta)
    let raw = fc.value + offset
    let v = raw.max(fc.min).min(fc.max)
    _FloatChoice(v, fc.min, fc.max)

  fun _perturb_bool(bc: _BoolChoice): _Choice val =>
    if bc.forced then return bc end
    _BoolChoice(not bc.value)

  fun _perturb_u128(uc: _U128Choice, rnd: Randomness ref): _Choice val =>
    if uc.min == uc.max then return uc end
    let range = uc.max - uc.min
    let delta = range / 4
    if delta == 0 then
      let v = rnd._raw_u128(uc.min, uc.max)
      return _U128Choice(v, uc.min, uc.max, uc.shrink_towards)
    end
    // Generate a random value in [0, 2*delta] then subtract delta
    // to get a signed perturbation within U128 space.
    let two_delta = delta * 2
    let raw_offset = rnd._raw_u128(0, two_delta)
    let v =
      if raw_offset >= delta then
        let pos = raw_offset - delta
        if (uc.max - uc.value) >= pos then
          uc.value + pos
        else
          uc.max
        end
      else
        let neg = delta - raw_offset
        if (uc.value - uc.min) >= neg then
          uc.value - neg
        else
          uc.min
        end
      end
    _U128Choice(v, uc.min, uc.max, uc.shrink_towards)
