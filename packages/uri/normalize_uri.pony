primitive NormalizeURI
  """
  Normalize a URI per RFC 3986 sections 6.2.2 (syntax-based) and 6.2.3
  (scheme-based).

  Syntax-based normalization (always safe, uses only generic URI syntax):
  - **Case normalization**: scheme and host are lowercased.
  - **Percent-encoding normalization**: hex digits in `%XX` are uppercased;
    percent-encoded unreserved characters are decoded to their literal form.
  - **Dot-segment removal**: `.` and `..` segments are resolved in the path.

  Scheme-based normalization (requires scheme-specific knowledge):
  - **Default port removal**: ports matching the scheme's well-known default
    (http→80, https→443, ftp→21) are removed.
  - **Empty path normalization**: for `http` and `https`, an empty path with
    an authority is set to `"/"` (per RFC 7230).

  Returns `InvalidPercentEncoding` if any component contains a malformed
  percent-encoded sequence, since `ParseURI` does not validate
  percent-encoding within components.
  """
  fun apply(uri: URI val): (URI val | InvalidPercentEncoding val) =>
    """
    Normalize `uri` and return the normalized form, or an error if
    percent-encoding is malformed.
    """
    // -- 6.2.2: Syntax-based normalization --

    // Lowercase scheme
    let norm_scheme: (String | None) =
      match \exhaustive\ uri.scheme
      | let s: String => s.lower()
      | None => None
      end

    // Normalize authority
    let norm_authority: (URIAuthority | None) =
      match \exhaustive\ uri.authority
      | let a: URIAuthority =>
        // Normalize userinfo percent-encoding
        let norm_userinfo: (String | None) =
          match \exhaustive\ a.userinfo
          | let u: String =>
            match \exhaustive\ _NormalizePercentEncoding(u)
            | let s: String => s
            | let e: InvalidPercentEncoding val => return e
            end
          | None => None
          end

        // Lowercase host, then normalize percent-encoding
        let lower_host: String val = a.host.lower()
        let norm_host =
          match \exhaustive\ _NormalizePercentEncoding(lower_host)
          | let s: String => s
          | let e: InvalidPercentEncoding val => return e
          end

        URIAuthority(norm_userinfo, norm_host, a.port)
      | None => None
      end

    // Normalize path: percent-encoding first, then dot-segment removal
    let pct_path =
      match \exhaustive\ _NormalizePercentEncoding(uri.path)
      | let s: String => s
      | let e: InvalidPercentEncoding val => return e
      end
    let norm_path = RemoveDotSegments(pct_path)

    // Normalize query percent-encoding
    let norm_query: (String | None) =
      match \exhaustive\ uri.query
      | let q: String =>
        match \exhaustive\ _NormalizePercentEncoding(q)
        | let s: String => s
        | let e: InvalidPercentEncoding val => return e
        end
      | None => None
      end

    // Normalize fragment percent-encoding
    let norm_fragment: (String | None) =
      match \exhaustive\ uri.fragment
      | let f: String =>
        match \exhaustive\ _NormalizePercentEncoding(f)
        | let s: String => s
        | let e: InvalidPercentEncoding val => return e
        end
      | None => None
      end

    // -- 6.2.3: Scheme-based normalization --
    // Only applies when a scheme is present (not for relative references)
    (let final_authority, let final_path) =
      match \exhaustive\ norm_scheme
      | let scheme: String =>
        // Default port removal
        let scheme_authority =
          match \exhaustive\ norm_authority
          | let a: URIAuthority =>
            match \exhaustive\ (a.port, _SchemeDefaultPort(scheme))
            | (let port: U16, let default_port: U16)
              if port == default_port
            =>
              URIAuthority(a.userinfo, a.host, None)
            else
              a
            end
          | None => None
          end

        // Empty path normalization (http and https only)
        let scheme_path =
          if (norm_path == "") and
            (scheme_authority isnt None) and
            ((scheme == "http") or (scheme == "https"))
          then
            "/"
          else
            norm_path
          end

        (scheme_authority, scheme_path)
      else
        (norm_authority, norm_path)
      end

    URI(
      norm_scheme,
      final_authority,
      final_path,
      norm_query,
      norm_fragment)

primitive _SchemeDefaultPort
  """
  Return the well-known default port for a URI scheme, or `None` for
  unknown schemes.

  The input scheme must already be lowercased (6.2.2 case normalization
  runs before 6.2.3).
  """
  fun apply(scheme: String): (U16 | None) =>
    if scheme == "http" then
      80
    elseif scheme == "https" then
      443
    elseif scheme == "ftp" then
      21
    else
      None
    end
