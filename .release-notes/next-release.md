## Add ability to trace runtime events in chromium json format #4649

We have added the ability to trace runtime events for pony applications. Runtime tracing is helpful in understanding how the runtime works and for debugging issues with the runtime.

The tracing can either write to file in the background or work in a "flight recorder" mode where events are stored into in memory circular buffers and written to stderr in case of abnormal program behavior (SIGILL, SIGSEGV, SIGBUS, etc).

Traces are written in chromium json format and trace files can be viewed with the perfetto trace viewer.

Links:
[Chromium json format](https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview?tab=t.0)
[perfetto trace viewer](https://perfetto.dev/)

