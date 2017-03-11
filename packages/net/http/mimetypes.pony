primitive MimeTypes
  """
  Provide mapping from file names to MIME types.
  TODO load from /etc/mime.types
  """
  fun apply(name: String): String val^ =>
    """
    Mapping is based on the file type, following the last period in the name.
    """
    try
      // This will fail if no period is found.
      let dotpos = (name.rfind(".", -1, 0)+1).usize()

      match name.trim(dotpos).lower()
      | "html" => "text/html"
      | "jpg"  => "image/jpeg"
      | "jpeg" => "image/jpeg"
      | "png"  => "image/png"
      | "css"  => "text/css"
      | "ico"  => "image/x-icon"
      | "js"   => "application/javascript"
      | "mp3"  => "audio/mpeg3"
      | "m3u"  => "audio/mpegurl"
      | "ogg"  => "audio/ogg"
      | "doc"  => "application/msword"
      | "gif"  => "image/gif"
      | "txt"  => "text/plain"
      | "wav"  => "audio/wav"
      else
        "application/octet-stream" // None of the above
      end
    else
      "application/octet-stream" // No filetype
    end


