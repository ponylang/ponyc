primitive ResolveURI
  """
  Resolve a URI reference against a base URI per RFC 3986 section 5.

  Given an absolute base URI and any URI-reference, produces the target URI
  that the reference identifies relative to the base. This is the operation
  browsers perform when resolving `href` attributes against the current
  document URL.

  The base must be an absolute URI (must have a scheme). Returns
  `BaseURINotAbsolute` if the base lacks a scheme.
  """
  fun apply(base: URI val, reference: URI val)
    : (URI val | ResolveURIError val)
  =>
    """
    Resolve `reference` against `base` using the algorithm from RFC 3986
    section 5.2.2. The base must have a scheme; the reference may be any
    URI-reference (absolute or relative).
    """
    let base_scheme =
      match \exhaustive\ base.scheme
      | let s: String => s
      | None => return BaseURINotAbsolute
      end

    match reference.scheme
    | let r_scheme: String =>
      // Reference has scheme — use it entirely (with dot-segment removal)
      URI(
        r_scheme,
        reference.authority,
        RemoveDotSegments(reference.path),
        reference.query,
        reference.fragment)
    else
      match reference.authority
      | let r_auth: URIAuthority =>
        // Reference has authority — inherit base scheme only
        URI(
          base_scheme,
          r_auth,
          RemoveDotSegments(reference.path),
          reference.query,
          reference.fragment)
      else
        if reference.path == "" then
          // Empty path — inherit base path; inherit base query only when
          // the reference has no query at all
          let q: (String | None) =
            match reference.query
            | let rq: String => rq
            else
              base.query
            end
          URI(
            base_scheme,
            base.authority,
            base.path,
            q,
            reference.fragment)
        else
          // Non-empty relative path
          let resolved_path =
            try
              if reference.path(0)? == '/' then
                // Absolute path — use directly with dot-segment removal
                RemoveDotSegments(reference.path)
              else
                // Relative path — merge with base then remove dot segments
                RemoveDotSegments(_merge(base, reference.path))
              end
            else
              _Unreachable()
              ""
            end
          URI(
            base_scheme,
            base.authority,
            resolved_path,
            reference.query,
            reference.fragment)
        end
      end
    end

  fun _merge(base: URI val, ref_path: String val): String val =>
    """
    RFC 3986 section 5.2.3: merge a relative-path reference with a base URI.
    """
    match base.authority
    | let _: URIAuthority =>
      if base.path == "" then
        // Base has authority and empty path — prepend "/"
        return "/" + ref_path
      end
    end
    // Take base path up to and including last "/", append reference path
    try
      var i = base.path.size()
      while i > 0 do
        i = i - 1
        if base.path(i)? == '/' then
          return base.path.substring(0, (i + 1).isize()) + ref_path
        end
      end
    else
      _Unreachable()
    end
    // No "/" in base path — reference path stands alone
    ref_path

primitive BaseURINotAbsolute is Stringable
  """
  The base URI passed to `ResolveURI` does not have a scheme.

  RFC 3986 section 5.2.2 requires the base URI to be an absolute URI.
  A relative reference cannot serve as a resolution base.
  """
  fun string(): String iso^ => "BaseURINotAbsolute".clone()

type ResolveURIError is BaseURINotAbsolute
