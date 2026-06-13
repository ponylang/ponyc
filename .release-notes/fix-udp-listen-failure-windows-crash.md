## Fix Windows process crash when a UDP socket fails to listen

On Windows, a `UDPSocket` that failed to start listening — for example because its host address could not be resolved — would crash the entire process immediately after reporting the failure through `not_listening`. The failure is now reported and your program keeps running, matching the behavior on other platforms.
