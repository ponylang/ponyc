// G[A].depth calls G[Array[A] val].depth only when A is U8. G[U8] recurses
// once and G[Array[U8] val] stops, so the program prints 1.
class G[A: Any val]
  fun depth(): USize =>
    iftype A <: U8 then
      G[Array[A] val].depth() + 1
    else
      0
    end

actor Main
  new create(env: Env) =>
    env.out.print(G[U8].depth().string())
