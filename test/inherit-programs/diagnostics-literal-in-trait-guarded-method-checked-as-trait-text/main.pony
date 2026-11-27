// T's mk has a guarded specialization whose object literal has a try that
// never errors. The literal is written in T's own method, in a method group,
// so T's check reports it, once and with no inherited-by note. The test
// expects that error.
interface Runner
  fun apply(): U8

trait T[A: Any val]
  fun mk(): Runner val => object val is Runner fun apply(): U8 => 0 end
  fun mk(): Runner val iftype A <: U8 val =>
    object val is Runner
      fun apply(): U8 =>
        try 1 else 2 end
    end

primitive P is T[String]

actor Main
  new create(env: Env) =>
    env.out.print(P.mk()().string())
