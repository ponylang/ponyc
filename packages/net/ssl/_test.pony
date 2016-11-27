use "net"
use "files"
use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestCertificateBuffer)

class _TestCertificateBuffer is UnitTest
  """
  Test SSL certificate buffer handling.
  """
  var certificate: String =
    "-----BEGIN CERTIFICATE-----\n" +
    "MIIFLjCCAxagAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwXjELMAkGA1UEBhMCVVMx\n" +
    "FzAVBgNVBAgMDk5vcnRoIENhcm9saW5hMRIwEAYDVQQKDAlBbGljZSBMdGQxIjAg\n" +
    "BgNVBAMMGUFsaWNlIEx0ZCBJbnRlcm1lZGlhdGUgQ0EwHhcNMTcwMTA5MTkzMDEx\n" +
    "WhcNMjYwOTI5MTkzMDExWjBUMQswCQYDVQQGEwJVUzEXMBUGA1UECAwOTm9ydGgg\n" +
    "Q2Fyb2xpbmExEjAQBgNVBAoMCUFsaWNlIEx0ZDEYMBYGA1UEAwwPd3d3LmV4YW1w\n" +
    "bGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1pwoTyHpHAZf\n" +
    "Wjb682q3LG505zmVMjqrkrSBjCVWHEhIjy38+BAZtpY/5WztSA7EvSmUBt8Xisy3\n" +
    "JMQCGeNHe/OpPVE5fOOAOJ84PQR4eiQtypOhnOvdyRhym3QuVXUNUzYKfnyLVS0U\n" +
    "4UiOsdNsPJkE4Wmpu3sv4hXTYLBKIM5W8zrHB6v8EiVtnk7QECl0nD5NCyTMsjZg\n" +
    "G7JROqOM1Y3XHktPEcATcf98QGR7BMgVRQjoL0ht04XJ2SRtDWANUj968JllcbZG\n" +
    "/tgUpuPvKSvHvRpypOzFKC5kqdmSXzSDnBnJfQW+27qwb6uXo/XTywHSyO5NyhYF\n" +
    "ND41xha8nQIDAQABo4H/MIH8MAkGA1UdEwQCMAAwEQYJYIZIAYb4QgEBBAQDAgZA\n" +
    "MDMGCWCGSAGG+EIBDQQmFiRPcGVuU1NMIEdlbmVyYXRlZCBTZXJ2ZXIgQ2VydGlm\n" +
    "aWNhdGUwHQYDVR0OBBYEFC7p3NXx0W4Hxwn5Zzyn6LqEu1QZMGMGA1UdIwRcMFqA\n" +
    "FN5PZJfFEHsyo+rprxRpXdpnE4y4oT6kPDA6MQswCQYDVQQGEwJVUzEXMBUGA1UE\n" +
    "CAwOTm9ydGggQ2Fyb2xpbmExEjAQBgNVBAoMCUFsaWNlIEx0ZIICEAAwDgYDVR0P\n" +
    "AQH/BAQDAgWgMBMGA1UdJQQMMAoGCCsGAQUFBwMBMA0GCSqGSIb3DQEBCwUAA4IC\n" +
    "AQBmGNgD3a6aMzn8GTRv/6qtGQCG3PHYArHaHB50Tz4ZOeLw68oOJwUADdFHZr2f\n" +
    "Xh/zdVJgYqQMpYCXswr5esfxUwYudTt8T38O+/uiSfP6YP38MFWa0Dxev0GKSfu8\n" +
    "PlxCDv1E8da5AN/Ddx/xXY4ZYzCN7e0nTlHDpwQoy0yI5HHHpTkV+U6QNs2toqWc\n" +
    "HZ4Lv5sVDcAYWN2idoJxMM/8cxSWsPsJUVAzAiH+VbbwqqYRTAPUn7wB0nqABEFf\n" +
    "nS6O+L3WGik2pBwSrfRK6WZ76PAgOLby34AqFHZVCtPh+p7lSqh0iZ3QZs1ns8N1\n" +
    "0atCOCZ765EItcDbRK3S7LbK9TJ6ipkX4dtEyf1o9Phq+2NQbLji1BDchaImEVo6\n" +
    "gYN3BxjSFcLSgbVfWT4+UBvBIQvRQiW3DwySQwVUqwb2j2qQ63EMcS+Q/Aen1sNw\n" +
    "tUQqD12MlUagD25ZxXYA/nNodJlGVVKy1v0pLlTyLLo5Lrj4xv1VH35nkO0z/S9s\n" +
    "KlfBlWXLkCbYtZhf/2Ld8tgI++G+7jxkkYMsqR5MwU2lWFF3lOoLHKvSu8oLUSw5\n" +
    "LYUs+QPR/8KszgsuRJ15H06+DCwMVsXS8mx6WgSZ0ZBUKMkhXJO1IsyT5cGyIaTe\n" +
    "cZ97LlHrD+yogZ3w32qz+v2q2mQidbvniHlwyE+pUGvf5g==\n" +
    "-----END CERTIFICATE-----\n" +
    "-----BEGIN CERTIFICATE-----\n" +
    "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\n" +
    "7c7AeHEOvOPWAWvzwAc=                                            \n" +
    "-----END CERTIFICATE-----                                       \n" +
    "-----BEGIN CERTIFICATE-----                                     \n" +
    "MIIFWjCCA0KgAwIBAgIJAI+0HzE/eN2hMA0GCSqGSIb3DQEBCwUAMDoxCzAJBgNV\n" +
    "BAYTAlVTMRcwFQYDVQQIDA5Ob3J0aCBDYXJvbGluYTESMBAGA1UECgwJQWxpY2Ug\n" +
    "THRkMB4XDTE3MDEwOTE5MDgzOVoXDTM3MDEwNDE5MDgzOVowOjELMAkGA1UEBhMC\n" +
    "VVMxFzAVBgNVBAgMDk5vcnRoIENhcm9saW5hMRIwEAYDVQQKDAlBbGljZSBMdGQw\n" +
    "ggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQC0SqnlPqwnwbTUzB7Kb7Xh\n" +
    "eZAIaeF+cP3aIynEQAgX2RQH0YCv5Sg1mBxgVZHxMrG1I8irIpOyhzANbodRVOFu\n" +
    "J1wkqUTfzYhUF0yK3Z3i+uojfZ56tX2/D5HEWAwTeXUGSp+03fJRsJvYPPNWpsqw\n" +
    "JB9+/1Gl8IoGXKvQU37wiWdnyOoaic6an49Ij6tw+72qQAHyRqbqBN6Nk/2MTOZl\n" +
    "lSn3WRoFhXSQPBf8EXaRxlUEqzu81SMok1e5Hn2SwZXjtGvsN51G4tqQ3czMFt5q\n" +
    "78nuCA7u9/IQxfvN3avwkCpzrRofxc7GY5vem/s8RaW/5BM138Vz5KkwffuECQw+\n" +
    "OSHiG19FnXtCl7h+S5Go9TW4vpAnUctyRmruOy6bnfaYlfBvJUVvWhnvC9pXG5f2\n" +
    "iTGaj/HHpEczo04HllfhJSilGrqeSEGxEJo9Qvz8EMrBSj/SN/ZziRFajlxf4yuk\n" +
    "onbYieACU6PfYmpHTgO9z415nILMchuApmqJL5zXQ20odq6YgogH199HWZLireF3\n" +
    "PQHYHiwD7HXV0QkMI3iLUmc5/Ee8gjUehHk62iQj4JC7584xveA7z0ontJI5O01O\n" +
    "uuiLCp1z4ZWcNZsott/MnOg9r4/6uQ34cDukIlOF+GoECWNXCWGVsGbYM8g3empv\n" +
    "P87GA1cRj/+T8gYexFQPQQIDAQABo2MwYTAdBgNVHQ4EFgQUEBTX7CNP4O0rqG9F\n" +
    "X3Vwks8CGYcwHwYDVR0jBBgwFoAUEBTX7CNP4O0rqG9FX3Vwks8CGYcwDwYDVR0T\n" +
    "AQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwDQYJKoZIhvcNAQELBQADggIBAFS5\n" +
    "wgciPBf0RjgAGJAUeIaDiG+7kW0HwGua80AboOrzwN6Lz4uM+ATNxOSUl61t9urE\n" +
    "c9ItX5WKGqmoORIASmD5CrnDJQz7Sbcds8nTDbRE9G/eyx6jgFxOnS6lIr1nxyCc\n" +
    "fKi0NldGdxzeTNKHSdVR7R8zCHrf7rzRF1sIunsb4ZFozIEhrmMWJNvDPxfgcZQy\n" +
    "f9Z/aql4Nob6AZQyL/8cvBvED8WuMIrn52klbFVcQ5RrwKuyJ9Q/olTou6lKufOR\n" +
    "5CevHDnZ+wbSiRjINSLZtfxCvrnjcnshxsvnXC0hfka0F/hygJS/Jvq58C8FyOQp\n" +
    "NJU4vbZ5u6JXvySBve6kL/YZS9Lc2S7rEo5bjX0o97POzFJcGNQFnym94OxAIK40\n" +
    "GOoV3IwH2j4X/i0uoKBe09oO7KvelaEhpLTQM5tlnn0G2IX827B6FBKLMjWDz37w\n" +
    "tuKzs68cFeDgCUb26cR040NW4kSwwjx2+OKfaDX9+DB2rdP5wMB3AbT3eC/wr7mJ\n" +
    "I1CIQXNc3dfQ3piNmmAlN2xR0A6fPLJVby5UmBNT8lfKUr89kW6fKXrz4Ffwukag\n" +
    "uqSv2M/U8qwBQz0ZTwJJqaJHGtozgyDrhdf2Y59FWRbi28JBeen0uJdN/xbZWkiK\n" +
    "oUujaksBtpiigLN/HSAWlwtm99u9nK4lK7oT6ybU\n" +
    "-----END CERTIFICATE-----\n"

  var key: String =
    "-----BEGIN RSA PRIVATE KEY-----\n" +
    "MIIEpAIBAAKCAQEA1pwoTyHpHAZfWjb682q3LG505zmVMjqrkrSBjCVWHEhIjy38\n" +
    "+BAZtpY/5WztSA7EvSmUBt8Xisy3JMQCGeNHe/OpPVE5fOOAOJ84PQR4eiQtypOh\n" +
    "nOvdyRhym3QuVXUNUzYKfnyLVS0U4UiOsdNsPJkE4Wmpu3sv4hXTYLBKIM5W8zrH\n" +
    "B6v8EiVtnk7QECl0nD5NCyTMsjZgG7JROqOM1Y3XHktPEcATcf98QGR7BMgVRQjo\n" +
    "L0ht04XJ2SRtDWANUj968JllcbZG/tgUpuPvKSvHvRpypOzFKC5kqdmSXzSDnBnJ\n" +
    "fQW+27qwb6uXo/XTywHSyO5NyhYFND41xha8nQIDAQABAoIBACgOLVfXtHKOne1w\n" +
    "pZYZLOcWFquxi688VBmlpyhJL7FHrINMyhJiruntUS+5DPCOERpdUEuYCATYALbT\n" +
    "/rBmQ0lXSRcwudIdhCkNTqnU08e4SPOualOnklWeQoXRQXShzELjq0HAbSEQsPz7\n" +
    "VebK10DYLpkD57IeY+mGuVUqkitdAEtpbGJJ4oaPdjWlmUZBnr+LfGpGeMVHyOd/\n" +
    "NHaIQtAAIzwHYbFjpusTjJWas36o5mHLENXmln21rkZPSfSJO5R/0ynvtLdsmNwS\n" +
    "vIqXh3tK4QLUjIevi95urn9JBqewkdf+7cUAsOxof0aVsl10b1tQVoikVCdlnVrP\n" +
    "0dpLsOECgYEA7j6kDEx9WiEh6DjdZPyLf3ujcsBRyq5fXISeTsmGj1fw8yewwGDN\n" +
    "HklZi/gNTLSl8lS5qfhDBZVzA8zvgrYmTGPITvPhhI8Hx4ejR02kGefYGtlphmaq\n" +
    "vetPb5dJFgM+0vJitKQNxL2yihrz2CBV3kfX2fOgx7pEjXz+YceO/I8CgYEA5pqZ\n" +
    "6jQaRdiHBV+A2Cj8yRGoKgkhotYetkCuobb//yjU6Wa6FPQk2GMya1G7R+a6ZFjs\n" +
    "PLAVuTBBvOxHU73LFIbBWaibx0l0uo3mxF0D2Yye7buPHYvcKBinGZ1fo7CyBDLS\n" +
    "6+mTkAcQcbGJhINQqgG/R9bqzqX3NRHjTy2hIhMCgYEAtDYT1u3A3/0x7bud1Dan\n" +
    "ul2mjDVnaR2vKodyS/xqWWFW9Eawj0Rpw6AOKS8PuFbpM73c1vHgmIesgVJyVXg8\n" +
    "zgmoSfh2PNQIOWijHFthZusVb3HHPY/JVXF6G9newdTgn6YS/bseYxRuycKLco87\n" +
    "BLFpWAbONmXyVugWb5YI1YcCgYEAnALoMmzBgZN19YqhLpy8OIP7k0TOPs5BxOeJ\n" +
    "vdqC4uuK78USC28JJmQmDjkjTQep/o2ydXRjngAp2Vi5AMycgeyRDsllarQlhrjN\n" +
    "8PdEzX+3XfrQXhrq/S2Aj3HK7IF0TibcT8KclIpS87g64y27+uhnsoCSJvBdiMfr\n" +
    "Uvv50YUCgYARll9vaYPhkxJMw0KkqhYDTOXjhe97A4P0Ux/f6FkRFm4c+ksTjpr7\n" +
    "3EoE0ofwwQ+Eq9JcXt9/ZfL9te6pTciU7+mCxwef9ltk0hgq/EJSchkhkgcZsdvV\n" +
    "V4Z7n70C19/9OR+ZyLgQp5OwdNBWiu9V2DimA4Xn9+47j0N2XPeiFA==\n" +
    "-----END RSA PRIVATE KEY-----\n"

  fun name(): String => "net/ssl/CertificateBuffer"
  fun exclusion_group(): String => "ssl"

  fun ref apply(h: TestHelper) =>
    let sslctx =
      recover
        let sc = SSLContext
        try
          sc.set_cert(certificate.array(), key.array())
        else
          h.fail("set_cert failed")
        end
        sc
      end
