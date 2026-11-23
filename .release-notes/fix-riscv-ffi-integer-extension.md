## Fix narrow integer C FFI calls on RISC-V

On RISC-V, C FFI calls now preserve narrow integer arguments and results according to the C calling convention. The previous behavior could corrupt values such as a TCP listener's port number, causing a client to connect to the wrong port.
