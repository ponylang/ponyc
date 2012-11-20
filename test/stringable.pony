trait StringBuffer
{
  function append( a:String )
  function appendn( a:String, n:U32 )
}

// FIX: should traits always be able to reference their concrete type?
trait Stringable[T]
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

  function string_pattern()->( r:\T )
  {
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
