interface Runner
  fun apply(): None

trait T
  fun v(): (U8 | String)

  fun mk(): Runner val =>
    let x = v()
    object val is Runner
      fun apply(): None =>
        match x
        | let u: U8 => None
        end
    end
