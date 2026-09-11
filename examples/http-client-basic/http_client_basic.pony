"""
Basic HTTP GET example.

Connects to `example.com:80`, sends an HTTP GET for `/`, and prints the
response status, headers, and body to stdout. Demonstrates the full lifecycle:
`HTTPClientConnectionActor` implementation, `on_connected` to send the request,
streaming body accumulation via `ResponseCollector`, and connection close after
completion. Start here if you're new to the library.
"""
