primitive _ToString
  """
  Worker type providing simple to string conversions for numbers.
  """
  fun _u64(x: U64, neg: Bool): String iso^ =>
    let table = "0123456789"
    let base: U64 = 10

    recover
      var s = String(31)
      var value = x

      try
        if value == 0 then
          s.push(table(0))
        else
          while value != 0 do
            let index = ((value = value / base) - (value * base))
            s.push(table(index.usize()))
          end
        end
      end

      if neg then s.push('-') end
      s.reverse_in_place()
    end

  fun _u128(x: U128, neg: Bool): String iso^ =>
    let table = "0123456789"
    let base: U128 = 10

    recover
      var s = String(31)
      var value = x

      try
        if value == 0 then
          s.push(table(0))
        else
          while value != 0 do
            let index = (value = value / base) - (value * base)
            s.push(table(index.usize()))
          end
        end
      end

      if neg then s.push('-') end
      s.reverse_in_place()
    end

  fun _f64(x: F64): String iso^ =>
    recover
      var s = String(31)
      var f = String(31).append("%g")

      ifdef windows then
        @_snprintf[I32](s.cstring(), s.space(), f.cstring(), x)
      else
        @snprintf[I32](s.cstring(), s.space(), f.cstring(), x)
      end

      s.recalc()
    end
