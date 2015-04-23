use "net"
use "collections"

primitive _X509Name
primitive _GeneralName
primitive _GeneralNameStack

primitive X509
  fun valid_for_host(cert: Pointer[X509], host: String): Bool =>
    """
    Checks if an OpenSSL X509 certificate is valid for a given host.
    """
    try
      for name in all_names(cert).values() do
        if _match_name(host, name) then
          return true
        end
      end
    end
    false

  fun common_name(cert: Pointer[X509]): String ? =>
    """
    Get the common name for the certificate. Raises an error if the common name
    contains any NULL bytes.
    """
    if cert.is_null() then
      error
    end

    let subject = @X509_get_subject_name[Pointer[_X509Name]](cert)
    let len = @X509_NAME_get_text_by_NID[I32](subject, I32(13), Pointer[U8],
      I32(0))

    if len < 0 then
      error
    end

    let common = recover String.reserve(len.u64()) end
    @X509_NAME_get_text_by_NID[I32](subject, I32(13), common.cstring(),
      len + 1)
    common.recalc()

    if common.size() != len.u64() then
      error
    end

    common

  fun all_names(cert: Pointer[X509]): Array[String] val =>
    """
    Returns an array of all names for the certificate. Any names containing
    NULL bytes are not included. This includes the common name and all subject
    alternate names.
    """
    let array = recover Array[String] end

    if cert.is_null() then
      return array
    end

    try
      array.push(common_name(cert))
    end

    let stack = @X509_get_ext_d2i[Pointer[_GeneralNameStack]](cert, I32(85),
      Pointer[U8], Pointer[U8])

    if stack.is_null() then
      return array
    end

    var name = @sk_pop[Pointer[_GeneralName]](stack)

    while not name.is_null() do
      var ptype = I32(0)
      let value = @GENERAL_NAME_get0_value[Pointer[U8] tag](name, &ptype)

      match ptype
      | 2 => // GEN_DNS
        // Check for V_ASN1_IA5STRING
        if @ASN1_STRING_type[I32](value) == 22 then
          try
            array.push(
              recover
                // Build a String from the ASN1 data.
                let data = @ASN1_STRING_data[Pointer[U8]](value)
                let len = @ASN1_STRING_length[I32](value)
                let s = String.from_cstring(data)

                // If it contains NULL bytes, don't include it.
                if s.size() != len.u64() then
                  error
                end

                consume s
              end)
          end
        end
      | 7 => // GEN_IPADD
        // Turn the IP address into a string.
        array.push(
          recover
            // Build a String from the ASN1 data.
            let data = @ASN1_STRING_data[Pointer[U8]](value)
            let len = @ASN1_STRING_length[I32](value)
            String.from_cstring(
              @os_ip_string[Pointer[U8]](data, len), 0, false)
          end)
      end

      @GENERAL_NAME_free[None](name)
      name = @sk_pop[Pointer[_GeneralName]](stack)
    end

    @sk_free[None](stack)
    array

  fun _match_name(host: String, name: String): Bool =>
    """
    Returns true if the name extracted from the certificate is valid for the
    given host.
    """
    if DNS.is_ip4(host) or DNS.is_ip6(host) then
      // If the host is a literal IP address, it must match exactly.
      return host == name
    end

    if host.compare(name, name.size(), 0, 0, true) == 0 then
      // If the names are the same ignoring case, they match.
      return true
    end

    try
      if name(0) == '*' then
        // The name has a wildcard. Must be followed by at least two
        // non-empty domain levels.
        if (name.size() < 3) or (name(1) != '.') or (name(2) == '.') then
          return false
        end

        try
          // Find the second domain level and make sure it's followed by
          // something other than a dot.
          let offset = name.find(".", 3)

          if name.at_offset(offset + 1) == '.' then
            return false
          end
        end

        // Get the host domain.
        let domain = host.find(".")

        // If the host domain is the wildcard domain ignoring case, they match.
        return host.compare(name, name.size() - 1, domain, 1, true) == 0
      end
    end

    false
