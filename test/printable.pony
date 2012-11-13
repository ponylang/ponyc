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
    // defaults to field.print_pattern() for Printable fields and to Undefined otherwise
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

trait StringBuffer
{
  function append( a:String )
  function appendn( a:String, n:U32 )
}

trait Stringable
{
  function string(
    pattern = string_pattern(),
    depth:U32 = 0,
    lead = "  ",
    seen = Map[Stringable, List[String]],
    fqname = List[String]( ["this"] )
    )->( r:String )
  {
    var buffer = StringBuffer
    string_buffer( buffer, pattern, depth, lead, seen, fqname )
    r = buffer.string()
  }

  function string_buffer(
    buffer:StringBuffer,
    pattern = string_pattern(),
    depth:U32 = 0,
    lead = "  ",
    seen = Map[Stringable, List[String]],
    fqname = List[String]( ["this"] )
    )
  {
    seen( this ) = fqname
    var mirror = reflect( pattern )

    buffer.append( mirror.typename() )
    buffer.append( " = " )

    if lead != ""
    {
      buffer.append( "\n" )
      buffer.appendn( lead, depth )
    }

    buffer.append( "{" )

    for name, value in mirror.fields()
    {
      match value
      {
        case pattern.field( name ) as v:Stringable
        {
          if lead != ""
          {
            buffer.append( "\n" )
            buffer.appendn( lead, depth + 1 )
          }

          buffer.append( name )
          buffer.append( ":" )

          match seen( v )
          {
            case None
            {
              v.print( out, sub, depth + 1, lead, seen, fqname.push( name ) )
            }

            case as s:String
            {
              fqname.string_buffer( buffer )
            }
          }
        }

        case {}
      }
    }

    if lead != ""
    {
      buffer.append( "\n" )
      buffer.appendn( lead, depth )
    }

    buffer.append( "}" )
  }

  function string_pattern()->( r:Pattern )
  {
    // defaults to field.print_pattern() for Printable fields and to Undefined otherwise
    r = reflect()

    for name, value in r.fields()
    {
      match value
      {
        case as v:Stringable { r.setfield( name, v.string_pattern() ) }
        case { r.setfield( name, Undefined ) }
      }
    }
  }
}

