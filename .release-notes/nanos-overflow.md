## Fixed an overflow in Time.nanos() values on Windows

Based on how were were calculating nanos based of the QueryPerformanceCounter and QueryPerformanceFrequency on Windows, it was quite likely that we would eventually overflow the U64 we used to represent nanos and "go backwards in time".
