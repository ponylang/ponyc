# TCP Open/Close Stress Test

Stress test that opens and closes a large number of TCP connections sending a small amount of data between them.

## How it stresses the runtime

- Heavy ASIO activity
- Lots of actor creation and destruction
