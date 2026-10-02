## Fix getsockopt_u32 failing on platforms that return sub-4-byte socket options

On some platforms, the kernel returns certain socket options in fewer than 4 bytes. For example, arm64 Windows returns IPv6 multicast options in fewer bytes, and macOS returns IPv4 multicast options as a 1-byte `u_char`. Previously, `getsockopt_u32` required exactly 4 bytes and reported an error when it received fewer. Now it correctly handles 1, 2, and 4-byte returns by zero-extending to `U32`.
