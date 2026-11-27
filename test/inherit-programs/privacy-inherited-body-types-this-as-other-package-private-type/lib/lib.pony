trait T
  fun g(): String
  fun f(): String =>
    let me = this
    let arr = [this; me]
    let t = (this, me)
    let lam = {(x: String): String => x + "!"}
    me.g() + arr.size().string() + t._1.g() + lam(this.g())
