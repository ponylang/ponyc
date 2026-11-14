use "collections"

use @pony_os_ip_string[Pointer[U8]](src: Pointer[U8], len: I32)
use @X509_get_subject_name[Pointer[_X509Name]](cert: Pointer[X509])
use @X509_NAME_get_text_by_NID[I32](name: Pointer[_X509Name], nid: I32,
  buf: Pointer[U8] tag, len: I32)
use @X509_get_ext_d2i[Pointer[_GeneralNameStack]](cert: Pointer[X509],
  nid: I32, crit: Pointer[I32], idx: Pointer[I32])
use @OPENSSL_sk_pop[Pointer[_GeneralName]](stack: Pointer[_GeneralNameStack])
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x"
use @sk_pop[Pointer[_GeneralName]](stack: Pointer[_GeneralNameStack])
  if "libressl"
use @GENERAL_NAME_get0_value[Pointer[_Asn1String] tag](
  name: Pointer[_GeneralName], ptype: Pointer[I32])
use @ASN1_STRING_type[I32](value: Pointer[_Asn1String] tag)
use @ASN1_STRING_get0_data[Pointer[U8]](value: Pointer[_Asn1String] tag)
use @ASN1_STRING_length[I32](value: Pointer[_Asn1String] tag)
use @GENERAL_NAME_free[None](name: Pointer[_GeneralName])
use @OPENSSL_sk_free[None](stack: Pointer[_GeneralNameStack])
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x"
use @sk_free[None](stack: Pointer[_GeneralNameStack])
  if "libressl"

primitive _X509Name
primitive _GeneralName
primitive _GeneralNameStack
primitive _Asn1String

primitive _DNSSanTag
primitive _IpSanTag
primitive _CommonNameTag

type _CertName is
  ((_DNSSanTag, String) | (_IpSanTag, String) | (_CommonNameTag, String))

primitive X509
  """
  Reads the names an OpenSSL X509 certificate is valid for, and matches a host
  against them. `Pointer[X509]` stands in for the C `X509 *`; the primitive
  itself holds nothing.
  """
  fun valid_for_host(cert: Pointer[X509], host: String): Bool =>
    """
    Checks whether an OpenSSL X509 certificate is valid for a given host,
    following RFC 6125. When the certificate has DNS SANs, the common name
    is not checked. IP hosts are matched only against iPAddress SANs; DNS
    hosts are matched against DNS SANs (and the CN when no DNS SAN exists).

    IPv6 addresses are compared as strings, so a non-canonical form like
    `0:0:0:0:0:0:0:1` does not match a canonical `::1` in the certificate.
    """
    let names = _all_typed_names(cert)
    let is_ip = DNS.is_ip4(host) or DNS.is_ip6(host)

    var has_dns_san = false
    for n in names.values() do
      match n
      | (_DNSSanTag, _) => has_dns_san = true
      end
    end

    for n in names.values() do
      match \exhaustive\ n
      | (_IpSanTag, let s: String) =>
        if is_ip and (host == s) then return true end
      | (_DNSSanTag, let s: String) =>
        if (not is_ip) and _match_name(host, s) then return true end
      | (_CommonNameTag, let s: String) =>
        if
          (not is_ip) and (not has_dns_san) and _match_name(host, s)
        then
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
    if cert.is_null() then error end

    let subject = @X509_get_subject_name(cert)
    let len =
      @X509_NAME_get_text_by_NID(subject, I32(13), Pointer[U8], I32(0))

    if len < 0 then error end

    let common = recover String(len.usize()) end
    @X509_NAME_get_text_by_NID(
      subject, I32(13), common.cstring(), len + 1)
    common.recalc()

    if common.size() != len.usize() then error end

    common

  fun all_names(cert: Pointer[X509]): Array[String] val =>
    """
    Returns an array of all names for the certificate. Any names containing
    NULL bytes are not included. This includes the common name and all subject
    alternate names.
    """
    let typed = _all_typed_names(cert)
    let array = recover Array[String](typed.size()) end
    for entry in typed.values() do
      match \exhaustive\ entry
      | (_DNSSanTag, let s: String) => array.push(s)
      | (_IpSanTag, let s: String) => array.push(s)
      | (_CommonNameTag, let s: String) => array.push(s)
      end
    end
    array

  fun _all_typed_names(cert: Pointer[X509]): Array[_CertName] val =>
    let array = recover Array[_CertName] end

    if cert.is_null() then
      return array
    end

    try
      array.push((_CommonNameTag, common_name(cert)?))
    end

    let stack =
      @X509_get_ext_d2i(cert, I32(85), Pointer[I32], Pointer[I32])

    if stack.is_null() then
      return array
    end

    var name =
      ifdef "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" then
        @OPENSSL_sk_pop(stack)
      elseif "libressl" then
        @sk_pop(stack)
      else
        compile_error "You must select an SSL version to use."
      end

    while not name.is_null() do
      var ptype = I32(0)
      let value =
        @GENERAL_NAME_get0_value(name, addressof ptype)

      match ptype
      | 2 => // GEN_DNS
        if @ASN1_STRING_type(value) == 22 then
          try
            let s: String val =
              recover val
                let data = @ASN1_STRING_get0_data(value)
                let len = @ASN1_STRING_length(value)
                let s' = String.copy_cstring(data)
                if s'.size() != len.usize() then error end
                s'
              end
            array.push((_DNSSanTag, s))
          end
        end
      | 7 => // GEN_IPADD
        let s: String val =
          recover val
            let data = @ASN1_STRING_get0_data(value)
            let len = @ASN1_STRING_length(value)
            String.from_cstring(@pony_os_ip_string(data, len))
          end
        array.push((_IpSanTag, s))
      end

      @GENERAL_NAME_free(name)
      ifdef "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" then
        name = @OPENSSL_sk_pop(stack)
      elseif "libressl" then
        name = @sk_pop(stack)
      else
        compile_error "You must select an SSL version to use."
      end
    end

    ifdef "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" then
      @OPENSSL_sk_free(stack)
    elseif "libressl" then
      @sk_free(stack)
    else
      compile_error "You must select an SSL version to use."
    end
    array

  fun _match_name(host: String, name: String): Bool =>
    """
    Matches a DNS hostname against a single certificate name,
    case-insensitively. Wildcards of the form `*.domain.tld` match a single
    non-empty label.
    """
    if DNS.is_ip4(host) or DNS.is_ip6(host) then
      return host == name
    end

    if (name.size() > 0) and (host.size() == name.size()) and
      (host.compare_sub(name, name.size(), 0, 0, true) is Equal)
    then
      return true
    end

    try
      if name(0)? == '*' then
        if (name.size() < 3) or (name(1)? != '.') or (name(2)? == '.') then
          return false
        end

        // RFC 6125 §6.4.3: at least two non-empty domain levels after *.
        let offset = name.find(".", 3)?
        if name.at_offset(offset + 1)? == '.' then
          return false
        end

        let domain = host.find(".")?

        if domain == 0 then
          return false
        end

        let host_tail = host.size() - domain.usize()
        let name_tail = name.size() - 1
        if host_tail != name_tail then
          return false
        end

        return
          host.compare_sub(name, name_tail, domain, 1, true) is Equal
      end
    end

    false
