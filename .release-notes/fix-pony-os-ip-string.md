## Fix pony_os_ip_string returning NULL for valid IP addresses

The runtime function `pony_os_ip_string` had an inverted check on the result of `inet_ntop`. Since `inet_ntop` returns non-NULL on success, the inverted condition meant the function returned NULL for every valid IP address and only attempted to use the result buffer on failure.

Any code that called `pony_os_ip_string` with a valid IPv4 or IPv6 address got NULL back. The most visible downstream effect was in the `ssl` library, where `X509.all_names()` calls this function to convert IP SANs from certificates into strings. The NULL result produced empty strings, corrupting the names array and breaking hostname verification for certificates that use IP SANs.
