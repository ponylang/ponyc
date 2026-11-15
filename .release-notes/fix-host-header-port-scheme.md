## Fix http_client omitting port from Host header regardless of scheme

`http_client` omitted the port from the `Host` header whenever it was `"80"` or `"443"`, regardless of the connection's scheme. An HTTP request to port 443 sent `Host: h` instead of `Host: h:443`, telling the server the port was 80. An HTTPS request to port 80 sent `Host: h` instead of `Host: h:80`, telling the server the port was 443.

The port is now omitted only when it is the default for the connection's scheme: 80 for HTTP, 443 for HTTPS.
