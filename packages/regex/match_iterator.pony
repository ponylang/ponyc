class MatchIterator is Iterator[Match]
  """
  MatchIterator allows for calling code to repeatedly perform the same match
  against a subject string as an iterator. This lets callers repeat the match 
  until no more matches exist.
  """
  let _regex: Regex box
  let _subject: String 
  var _offset: USize = 0

  new create(regex': Regex box, subject': String, offset': USize = 0) =>   
    """
    Creates a new Match Iterator from a regular expression and a subject 
    string. 
    """ 
    _regex = regex'
    _subject = consume subject'
    _offset = offset'

  fun has_next() : Bool =>
    """
    Indicates whether there is another match available.
    """
    try
      let m = _regex(_subject, _offset)?
      true
    else
      false
    end 

  fun ref next(): Match ? =>
    """
    Yields the next match to the regular expression or produces
    an error if there is no match.
    """
    let m = _regex(_subject, _offset)?
    _offset = m.end_pos() + 1 
    m
    