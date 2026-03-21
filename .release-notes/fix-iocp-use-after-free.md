## Fix use-after-free crash in IOCP runtime on Windows

When a Pony actor closed a TCP connection on Windows, pending I/O completions could fire after the connection's ASIO event and owning actor were already freed, causing an intermittent access violation crash. The runtime now detects these orphaned completions and cleans them up safely without touching freed memory.
