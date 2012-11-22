trait StringBuffer
{
  function append( a:String )
  function appendn( a:String, n:U32 )
}

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
    // builds the string representation in a buffer and then turns the buffer
    // into an actual string
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
    // keep track of the fully qualified name of 'this'
    seen( this ) = fqname

    // reflect the fields that match the pattern
    var mirror = reflect( pattern )

    // begin with a type name
    buffer.append( mirror.typename() )
    buffer.append( " = " )

    if lead != ""
    {
      // if we're using indentation, indent on a new line
      buffer.append( "\n" )
      buffer.appendn( lead, depth )
    }

    // opening delimiter
    buffer.append( "{" )

    // iterate through our fields
    for name, value in mirror.fields()
    {
      // get the subpattern for this field
      var subpattern = pattern.field( name )

      match value
      {
        case subpattern as v:Stringable
        {
          // the field matches the subpattern and is Stringable
          if lead != ""
          {
            // if we're using indentation, indent on a new line
            buffer.append( "\n" )
            buffer.appendn( lead, depth + 1 )
          }

          // the name of the field and a ':'
          // this will be followed by a type name or a fully qualified name
          buffer.append( name )
          buffer.append( ":" )

          match seen( v )
          {
            case None
            {
              // we haven't seen v before, so add it to the buffer
              v.string_buffer(
                buffer, subpattern, depth + 1, lead, seen, fqname.push( name )
                )
            }

            case as s:List[String]
            {
              // we've seen v before, so just add the previous name
              s.string_buffer( buffer )
            }
          }
        }

        // doesn't match the pattern or isn't Stringable, leave it out
        case {}
      }
    }

    if lead != ""
    {
      // if we're using indentation, indent on a new line
      buffer.append( "\n" )
      buffer.appendn( lead, depth )
    }

    // closing delimiter
    buffer.append( "}" )
  }

  function string_pattern()->( r:Partial[T] )
  {
    // FIX:
  }
}
