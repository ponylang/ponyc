primitive Helper
  fun tag tagit(s: String): String => "lib:" + s

trait T
  fun show(s: String, f: {(String): String} val = {(x: String): String => Helper.tagit(x)}): String =>
    f(s)
