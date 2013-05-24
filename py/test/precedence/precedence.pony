trait Test
{

}

class Precedence[T:Test[N], U:Test|Test]
{
  /* nested
  /* comments */
  work */
  def assoc() I32 = 1_ + 77 * 2.0e-2 * 0xF_F / 0b1_1 % 01_2

  def assoc() F32 = 1 * 2 + 3

  def logic() Bool = a and b or c xor d

  def optarg() None =
    obj.invoke(arg, opt=if 3 > 4 then "te\nst" else "real")
}

class Foo[T]
{
  var a:Aardvark[T]<wri>

  def set<iso|wri>(a:Aardvark[T]<this>) None = None
  def get<wri|fro>() Aardvark[T]<this> = a
}
