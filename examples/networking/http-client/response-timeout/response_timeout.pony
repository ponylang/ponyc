"""
Response timeout using one-shot timers.

Connects to `httpbin.org` over HTTPS, sends a GET request for `/delay/10` (a
10-second delayed response), and sets a 3-second response deadline. Since the
server takes longer than the deadline, the timer fires and the connection is
closed with a timeout message. Demonstrates `HTTPClientConnection.set_timer()`,
`HTTPClientConnection.cancel_timer()`, and `on_timer()` in a response deadline
pattern.
"""
