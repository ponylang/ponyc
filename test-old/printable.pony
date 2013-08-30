/*
Type =
{
  field1:Type1 =
  {
    somestring:String = "A string"
    someint:I32 = 23
  }
  field2:Type2 = field.that.was.written.out.earlier
}
*/

trait Printable
{
  function print(
    out = System.stdout(),
    pattern = print_pattern(),
    depth:U32 = 0,
    lead = "  ",
    seen = Map[Printable, String],
    fqname = "this"
    )
  {
    seen( this ) = fqname
    var mirror = reflect( pattern )

    out.println( mirror.typename() + " = " )
    out.printn( depth, lead )
    out.println( "{" )

    for name, value in mirror.fields()
    {
      match value, value
      {
        case \Printable as v, pattern.field( name )
        {
          out.printn( depth + 1, lead )
          out.print( name + ":" )

          match seen( v )
          {
            case None
            {
              v.print( out, sub, depth + 1, lead, seen, fqname + "." + name )
            }

            case \String as s
            {
              out.print( s )
            }
          }

          out.println()
        }

        case {}
      }
    }

    out.printn( depth, lead )
    out.println( "}" )
  }

  function print_pattern()->( r:Pattern )
  {
    r = reflect()

    for name, value in r.fields()
    {
      match value
      {
        case \Printable as v { r.setfield( name, v.print_pattern() ) }
        case { r.setfield( name, Undefined ) }
      }
    }
  }
}
