trait Test
{

}

class Precedence[T:Test[N], U:Test|Test]
{
  /* nested
  /* comments */
  work */
  read assoc()->(I32) = 1_ + 77 * 2.0e-2 * 0xF_F / 0b1_1 % 01_2

  read assoc()->(F32) = 1 * 2 + 3

  read logic()->(Bool) = a and b or c xor d

  read optarg() =
    obj.invoke(arg, opt=if 3 > 4 then "te\nst" else "real")
}

class Foo[T]
{
  var a:Aardvark[T]<wri>

  write set(a:Aardvark[T]<wri>) = None
  read get()->(Aardvark[T]<wri> this) = a
}
